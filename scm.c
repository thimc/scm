/* See LCENSE file for copyright and license details. */

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include "config.h"
#include "util.h"

void sortentries(void);
int duplicate_text(const char *text);
void makelinecache(void);
char *getlinepreview(const char *path);
int scanentries(void);
int storeclip(char *text);
void usage(void);

int flags;
entries *clip;
static xorg instance;

void
sortentries(void)
{
	int i, j;
	entry *tmp;

	for (i = 0; i < (int)clip->len - 1; i++) {
		for (j = 0; j < (int)clip->len - i - 1; j++) {
			if (clip->entries[j]->fname > clip->entries[j + 1]->fname) {
				tmp = clip->entries[j];
				clip->entries[j] = clip->entries[j + 1];
				clip->entries[j + 1] = tmp;
			}
		}
	}
	debug("scanentries(): sorted");
}

int
duplicate_text(const char *text)
{
	int i;
	char *buf;
	entry *curr;

	buf = strndup(text, LINE_SIZE);
	buf[strcspn(buf, "\n")] = '\0';

	for (i = 0; i < (int)clip->len; i++) {
		curr = clip->entries[i];
		if (strcmp(curr->line, buf) == 0) {
			/* TODO: The first line may match but the rest of the
			 * clip entry may as well differ */
			free(buf);
			return 1;
		}
	}
	free(buf);

	return 0;
}

void
makelinecache(void)
{
	char path[PATH_SIZE], cpath[PATH_SIZE];
	FILE *f;
	int i;

	snprintf(cpath, PATH_SIZE, "%s/line_cache", maindir);
	debug("makelinecache(): file path: \"%s\"", cpath);

	if ((f = fopen(cpath, "w")) == NULL) {
		die("fopen(): %s '%s'", path, strerror(errno));
	}
	for (i = (int)clip->len - 1; i >= 0; i--) {
		snprintf(path, PATH_SIZE, "%s/E%d", maindir, clip->entries[i]->fname);
		if (access(path, F_OK) < 0) {
			continue;
		}
		fprintf(f, "%s\t%s\n", path, clip->entries[i]->line);
		debug("makelinecache(): %s\t%s", path, clip->entries[i]->line);
	}

	fclose(f);
}

char *
getlinepreview(const char *path)
{
	FILE *f;
	char *line;
	int ch, nlines = 0;

	line = ecalloc(sizeof(&line), LINE_SIZE);

	debug("getlinepreview(): %s", path);

	if ((f = fopen(path, "r")) == NULL) {
		die("fopen(): %s '%s'", path, strerror(errno));
	}
	while ((ch = fgetc(f)) != EOF) {
		nlines += (ch == '\n');
	}
	rewind(f);

	if ((fgets(line, LINE_SIZE - LINE_COUNTER_SIZE, f)) == NULL) {
		die("fgets(): %s '%s'", path, strerror(errno));
	}
	fclose(f);
	line[strcspn(line, "\n")] = '\0';

	if (nlines > 1) {
		snprintf(line, LINE_SIZE, "%s (%d lines)", line, nlines);
	}
	return line;
}

int
scanentries(void)
{
	int r, i, pr;
	DIR *mdir;
	struct dirent *dent;
	char path[PATH_SIZE];

	if (!(mdir = opendir(maindir))) {
		die("opendir(): %s '%s'", maindir, strerror(errno));
	}
	while ((dent = readdir(mdir))) {
		if (!sscanf(dent->d_name, "E%d", &r)) {
			continue;
		}

		for (i = 0; i < (int)clip->len; i++) {
			if (clip->entries[i]->fname == r) {
				debug("scanentires(): duplicate %d", r);
				goto skip;
			}
		}
		if (r == pr) {
			continue;
		}

		if (clip->len < clip->capacity) {
			snprintf(path, PATH_SIZE, "%s/E%d", maindir, r);
			clip->entries[clip->len] = ecalloc(1, sizeof(&clip->entries[0]));
			clip->entries[clip->len]->fname = r;
			clip->entries[clip->len]->line = getlinepreview(path);

			debug("scanentries(): assigned \"E%d\" to entries[%ld]",
					r, clip->len);
			clip->len++;

		} else {
			pr = clip->entries[0]->fname;
			snprintf(path, PATH_SIZE, "%s/E%d", maindir, pr);
			if (!(flags & FLAG_KEEP)) {
				if (remove(path) < 0) {
					die("remove(): '%s' %s", path, strerror(errno));
				}
				debug("scanentries(): removed \"%s\"", path);
			} else {
				debug("scanentries(): kept \"%s\"", path);
			}

			free(clip->entries[0]->line);
			snprintf(path, PATH_SIZE, "%s/E%d", maindir, r);
			clip->entries[0]->fname = r;
			clip->entries[0]->line = getlinepreview(path);
			debug("scanentries(): replaced with \"%s\"", path);

			sortentries();
		}
skip:
		r = 0;
	}
	closedir(mdir);

	return 1;
}

int
storeclip(char *text)
{
	FILE *f;
	char path[PATH_SIZE];

	if (strnlen(text, LINE_SIZE) < 1) {
		return 1;
	}
	if (duplicate_text(text)) {
		return 1;
	}
	snprintf(path, PATH_SIZE, "%s/E%d", maindir, (int) time(NULL));
	if ((f = fopen(path, "w")) == NULL) {
		die("fopen(): %s '%s'", path, strerror(errno));
	}
	fputs(text, f);
	fclose(f);

	if (chmod(path, FILEMASK) < 0) {
		die("chmod(): %s '%s'", path, strerror(errno));
	}
	debug("storeclip(): wrote and chmod \"%s\"", path);

	return 0;
}

void
usage(void)
{
	fprintf(stderr, "usage: scm [-d directory] [-v] [-V] [-p] [-1] [-k]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *clipboard;
	char *primary;
	XFixesSelectionNotifyEvent *sel_event;
	XEvent event;
	int i, fd;
	char path[PATH_SIZE];

	flags = 0;
	maindir = NULL;
	if (!(instance.dpy = XOpenDisplay(NULL))) {
		die("can't open X display: %s", strerror(errno));
	}
	instance.root = DefaultRootWindow(instance.dpy);
	instance.clipboard = XInternAtom(instance.dpy, "CLIPBOARD", False);
	instance.primary = XInternAtom(instance.dpy, "PRIMARY", False);
	instance.win = XCreateSimpleWindow(instance.dpy, instance.root,
	    1, 1, 1, 1, 1, CopyFromParent, CopyFromParent);

#ifdef __OpenBSD__
	if (pledge("stdio fattr rpath wpath cpath flock unveil", NULL) < 0) {
		die("pledge(): %s", strerror(errno));
	}
#endif

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d")) {
			maindir = argv[++i];
			debug("working dir set to %s", maindir);
			if (access(maindir, F_OK) < 0) {
				die("access(): %s '%s'", path, strerror(errno));
			}
#ifdef __OpenBSD__
			if (unveil(maindir, "rwc") < 0) {
				die("unveil(): %s '%s'", path, strerror(errno));
			}
#endif
		} else if (!strcmp(argv[i], "-p")) {
			flags |= FLAG_PRIMARY;
		} else if (!strcmp(argv[i], "-v")) {
			puts("scm-" VERSION " © 2023 Thim Cederlund");
			puts("https://github.com/thimc/scm");
			exit(0);
		} else if (!strcmp(argv[i], "-V")) {
			flags |= FLAG_VERBOSE;
		} else if (!strcmp(argv[i], "-1")) {
			flags |= FLAG_ONESHOT;
		} else if (!strcmp(argv[i], "-k")) {
			flags |= FLAG_KEEP;
		} else {
			usage();
		}
	}

	if (!maindir)
		exit(1);

	snprintf(path, PATH_SIZE, "%s/lock", maindir);
	if ((fd = open(path, O_RDWR | O_CREAT, FILEMASK)) < 0) {
		die("open(): %s '%s'", path, strerror(errno));
	}
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		if (errno == EWOULDBLOCK) {
			die("%s is already running!", argv[0]);
		} else {
			die("flock(): %s '%s'", path, strerror(errno));
		}
	}
	if (XFixesQueryExtension(instance.dpy, &instance.event_base,
		&instance.error_base) == 0) {
		die("xfixes(): '%s'", strerror(errno));
	}
	XFixesSelectSelectionInput(instance.dpy, instance.root,
	    instance.clipboard, XFixesSetSelectionOwnerNotifyMask);

	if (flags & FLAG_PRIMARY) {
		XFixesSelectSelectionInput(instance.dpy, instance.root,
		    instance.primary, XFixesSetSelectionOwnerNotifyMask);
	}
	clip = ecalloc(1, sizeof(entries));
	clip->capacity = MAXENTRIES;
	clip->len = 0;
	clip->entries = ecalloc(clip->capacity, sizeof(&clip->entries[0]));

	do {
		if (scanentries()) {
			makelinecache();
		}
		XNextEvent(instance.dpy, &event);

		if (event.type == (instance.event_base + XFixesSelectionNotify)) {
			sel_event = (XFixesSelectionNotifyEvent *) & event;

			if (sel_event->selection == instance.clipboard) {
				if ((clipboard = get_utf_prop(instance, instance.clipboard))) {
					if (!storeclip(clipboard)) {
						goto skip;
					}
				}
			} else if (sel_event->selection ==
			    instance.primary && (flags & FLAG_PRIMARY)) {
				if ((primary = get_utf_prop(instance,
					    instance.primary))) {
					if (!storeclip(primary)) {
						goto skip;
					}
				}
			}
		}
skip:
		free(clipboard);
		/* TODO: sleep() is used avoid filename clashing */
		sleep(1);
	} while (!(flags & FLAG_ONESHOT));

	for (i = 0; i < (int)clip->capacity; i++) {
		free(clip->entries[i]->line);
		free(clip->entries[i]);
	}
	free(clip->entries);
	free(clip);

	if (flock(fd, LOCK_UN) < 0) {
		die("flock(): %s '%s'", path, strerror(errno));
	}
	XDestroyWindow(instance.dpy, instance.win);
	XCloseDisplay(instance.dpy);

	return 0;
}

// vim:ts=4 tw=4 cc=80
