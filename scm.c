/* See LICENSE file for copyright and license details. */
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include "util.h"
#include "config.h"

#define EPATHSIZE	 50
#define ELINESIZE	 80
#define ELINECTRSIZE 15

int duplicate(const char *text);
int getentries(void);
int cmp(const void *a, const void *b);
char *getlinepreview(const char *path);
void makelinecache(void);
void trimlist(void);
void sortentries(void);
int scanentries(void);
int storeclip(const char *text);
void usage(void);

int flags;
entry *clipentries;
static xorg instance;

int
duplicate(const char *text)
{
	int i;

	for (i = 0; i < MAXENTRIES; i++) {
		if (!clipentries[i].line)
			continue;
		if (strcmp(clipentries[i].line, text) == 0) {
			debug("duplicate entry: '%s' matched index %d", text, i);
			return 1;
		}
	}

	return 0;
}

int
cmp(const void *a, const void *b)
{
	return ((const entry *) a)->fname > ((const entry *) b)->fname;
}

void
makelinecache(void)
{
	char path[EPATHSIZE], cpath[EPATHSIZE];
	FILE *f;
	int i;

	snprintf(cpath, EPATHSIZE, "%s/line_cache", maindir);
	debug("generating line cache to %s", cpath);
	if ((f = fopen(cpath, "w")) == NULL)
		die("%s: fopen: %s '%s'", __func__, path, strerror(errno));

	qsort(clipentries, MAXENTRIES, sizeof(entry), cmp);
	for (i = 0; i < MAXENTRIES; i++) {
		snprintf(path, EPATHSIZE, "%s/E%d", maindir, clipentries[i].fname);
		debug("E%d\t%s", clipentries[i].fname, clipentries[i].line);
		if (access(path, F_OK) < 0)
			continue;
		fprintf(f, "%s\t%s\n", path, clipentries[i].line);
	}
	fclose(f);
}


char *
getlinepreview(const char *path)
{
	FILE *f;
	char *line, buf[ELINESIZE - ELINECTRSIZE];
	int ch, nlines = 0;

	if ((f = fopen(path, "r")) == NULL)
		die("%s: fopen: %s '%s'", __func__, path, strerror(errno));

	while ((ch = fgetc(f)) != EOF)
		nlines += (ch == '\n');
	rewind(f);

	if ((fgets(buf, sizeof(buf), f)) == NULL)
		die("%s: fgets: %s '%s'", __func__, path, strerror(errno));
	buf[strcspn(buf, "\n")] = '\0';

	line = ecalloc(ELINESIZE, sizeof(char));
	if (nlines) {
		snprintf(line, ELINESIZE, "%s (%d lines)", buf, nlines + 1);
	} else {
		memcpy(line, buf, ELINESIZE);
	}

	return line;
}

void
readentries(void)
{
	int i;
	char path[EPATHSIZE];

	for (i = 0; i < MAXENTRIES; i++) {
		if (!clipentries[i].fname)
			continue;
		snprintf(path, EPATHSIZE, "%s/E%d", maindir, clipentries[i].fname);
		if (access(path, F_OK) < 0)
			die("%s: access: %s '%s'", __func__, path, strerror(errno));
		clipentries[i].line = getlinepreview(path);
	}
}

int
scanentries(void)
{
	int r, f;
	DIR *mdir;
	struct dirent *dent;
	char path[EPATHSIZE];

	if (!(mdir = opendir(maindir)))
		die("%s: opendir: %s '%s'", __func__, maindir, strerror(errno));

	f = 0;
	while ((dent = readdir(mdir))) {
		if (!sscanf(dent->d_name, "E%d", &r))
			continue;

		if (f < MAXENTRIES) {
			clipentries[f++].fname = r;
		} else {
			qsort(clipentries, (size_t) f, sizeof(entry), cmp);
			if (r > clipentries[0].fname) {
				snprintf(path, EPATHSIZE, "%s/E%d", maindir, clipentries[0].fname);
				if (remove(path) < 0)
					die("%s: remove: '%s' %s", __func__, path, strerror(errno));

				clipentries[0].fname = r;

			} else {
				snprintf(path, EPATHSIZE, "%s/E%d", maindir, r);
				debug("excess file: %s", path);
				remove(path);
			}
		}
	}
	closedir(mdir);
	debug("found %d entries", f);

	return f;
}

int
storeclip(const char *text)
{
	FILE *f;
	char path[EPATHSIZE];

	if (duplicate(text))
		return 1;

	if (strnlen(text, ELINESIZE) < 1)
		return 1;

	snprintf(path, EPATHSIZE, "%s/E%d", maindir, (int) time(NULL));
	if ((f = fopen(path, "w")) == NULL)
		die("%s: fopen %s '%s'", __func__, path, strerror(errno));

	fprintf(f, "%s", text);
	fclose(f);

	if (chmod(path, FILEMASK) < 0)
		die("%s: chmod %s '%s'", __func__, path, strerror(errno));

	return 0;
}

void
usage(void)
{
	fprintf(stderr, "usage: scm [-d directory] [-v] [-V] [-p] [-1]\n");
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
	char path[EPATHSIZE];

	flags = 0;
	maindir = NULL;
	if (!(instance.dpy = XOpenDisplay(NULL)))
		die("%s: can't open X display: %s", __func__, strerror(errno));
	instance.root = DefaultRootWindow(instance.dpy);
	instance.clipboard = XInternAtom(instance.dpy, "CLIPBOARD", False);
	instance.primary = XInternAtom(instance.dpy, "PRIMARY", False);
	instance.win = XCreateSimpleWindow(instance.dpy, instance.root,
	    1, 1, 1, 1, 1, CopyFromParent, CopyFromParent);

#ifdef __OpenBSD__
	if (pledge("stdio fattr rpath wpath cpath flock unveil", NULL) < 0)
		die("%s: pledge: %s", __func__, strerror(errno));
#endif

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d")) {	/* set main directory */
			maindir = argv[++i];
			debug("main dir set to %s", maindir);
			if (access(maindir, F_OK) < 0)
				die("%s: access %s '%s'", __func__, path, strerror(errno));
#ifdef __OpenBSD__
			if (unveil(maindir, "rwc") < 0)
				die("%s: unveil %s '%s'", __func__, path, strerror(errno));
#endif
		} else if (!strcmp(argv[i], "-p")) {	/* PRIMARY changes */
			flags |= FLAG_PRIMARY;
		} else if (!strcmp(argv[i], "-v")) {	/* prints version
							 * information */
			puts("scm-" VERSION " © 2023 Thim Cederlund");
			exit(0);
		} else if (!strcmp(argv[i], "-V")) {
			flags |= FLAG_VERBOSE;
		} else if (!strcmp(argv[i], "-1")) {
			flags |= FLAG_ONESHOT;
		} else {
			usage();
		}
	}

	if (!maindir)
		exit(1);

	snprintf(path, EPATHSIZE, "%s/lock", maindir);
	if ((fd = open(path, O_RDWR | O_CREAT, FILEMASK)) < 0)
		die("%s: open: %s '%s'", __func__, path, strerror(errno));

	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		if (errno == EWOULDBLOCK)
			die("%s is already running!", argv[0]);
		else
			die("%s: flock: %s '%s'", __func__, path, strerror(errno));
	}
	if (XFixesQueryExtension(instance.dpy, &instance.event_base,
		&instance.error_base) == 0) {
		die("%s: xfixes '%s'", __func__, strerror(errno));
	}
	XFixesSelectSelectionInput(instance.dpy, instance.root,
	    instance.clipboard, XFixesSetSelectionOwnerNotifyMask);

	if (flags & FLAG_PRIMARY) {
		XFixesSelectSelectionInput(instance.dpy, instance.root,
		    instance.primary, XFixesSetSelectionOwnerNotifyMask);
	}
	clipentries = ecalloc(MAXENTRIES, sizeof(entry));

	do {
		debug("scanning local entries");
		if (scanentries()) {
			readentries();
			makelinecache();
		}
		debug("waiting for clipboard changes");
		XNextEvent(instance.dpy, &event);

		if (event.type == (instance.event_base + XFixesSelectionNotify)) {
			sel_event = (XFixesSelectionNotifyEvent *) & event;

			if (sel_event->selection == instance.clipboard) {
				if ((clipboard = get_utf_prop(instance, instance.clipboard))) {
					if (!storeclip(clipboard))
						goto skip;
				}
			} else if (sel_event->selection == instance.primary &&
			    (flags & FLAG_PRIMARY)) {
				if ((primary = get_utf_prop(instance, instance.primary))) {
					if (storeclip(primary) < 0)
						goto skip;
				}
			}
		}
skip:
		fflush(stdout);
		sleep(1);	/* TODO: to avoid filename clashing */
	} while (!(flags & FLAG_ONESHOT));

	for (i = 0; i < MAXENTRIES; i++) {
		free(clipentries[i].line);
	}
	free(clipentries);
	free(clipboard);

	if (flock(fd, LOCK_UN) < 0)
		die("%s: flock: %s '%s'", __func__, path, strerror(errno));

	XDestroyWindow(instance.dpy, instance.win);
	XCloseDisplay(instance.dpy);

	return 0;
}
//vim:cc = 80 ts = 4 tw = 4
