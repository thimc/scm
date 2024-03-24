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
int storentry(char *text);
void usage(char *argv);

int flags;
entries clip;
static context ctx;

void
sortentries(void)
{
	size_t i, j;
	entry tmp;

	for (i = 0; i < clip.count; i++) {
		for (j = 0; j < clip.count - i - 1; j++) {
			if (clip.entries[j].fname > clip.entries[j + 1].fname) {
				tmp = clip.entries[j];
				clip.entries[j] = clip.entries[j + 1];
				clip.entries[j + 1] = tmp;
			}
		}
	}
}

int
duplicate_text(const char *text)
{
	char path[PATH_SIZE];
	FILE *f;
	size_t i;
	char *buf;
	long siz;

	for (i = 0; i < clip.count; i++) {
		snprintf(path, PATH_SIZE, "%s/E%d", maindir, clip.entries[i].fname);
		if ((f = fopen(path, "r")) == NULL) return 1;
		fseek(f, 0, SEEK_END);
		siz = ftell(f);
		fseek(f, 0, SEEK_SET);
		buf = ecalloc(sizeof(*buf), siz + 1);
		fread(buf, sizeof(*buf), siz, f);
		buf[siz] = '\0';
		fclose(f);

		if (strcmp(text, buf) == 0) {
			free(buf);
			return 1;
		}
		free(buf);
	}

	return 0;
}

void
makelinecache(void)
{
	char path[PATH_SIZE], cpath[PATH_SIZE];
	FILE *f;
	int i;

	snprintf(cpath, PATH_SIZE, "%s/line_cache", maindir);

	if ((f = fopen(cpath, "w")) == NULL)
		die("makelinecache(): fopen(): %s '%s'", path, strerror(errno));
	for (i = clip.count - 1; i >= 0; --i) {
		snprintf(path, PATH_SIZE, "%s/E%d", maindir, clip.entries[i].fname);
		if (access(path, F_OK) < 0) continue;
		fprintf(f, "%s\t%s\n", path, clip.entries[i].line);
	}

	fclose(f);
}

char *
getlinepreview(const char *path)
{
	FILE *f;
	char *line, ln[LINE_COUNTER_SIZE];
	int ch, nlines, siz;

	nlines = 0;
	line = ecalloc(sizeof(*line), LINE_SIZE);
	if ((f = fopen(path, "r")) == NULL)
		die("getlinepreview(): fopen(): %s '%s'", path, strerror(errno));
	while ((ch = fgetc(f)) != EOF)
		nlines += (ch == '\n');
	fseek(f, 0, SEEK_SET);
	fgets(line, LINE_SIZE, f);
	fclose(f);
	siz = strlen(line);
	line[strcspn(line, "\n")] = '\0';
	line[LINE_SIZE - LINE_COUNTER_SIZE] = '\0';

	if (nlines++ > 0) {
		snprintf(ln, sizeof(ln), " (%d lines)", nlines);
	} else if (siz > LINE_SIZE - LINE_COUNTER_SIZE) {
		snprintf(ln, sizeof(ln), " ..");
	} else {
		goto ok;
	}

	strncat(line, ln, sizeof(ln));
	line[LINE_SIZE-1] = '\0';

ok:
	return line;
}

int
scanentries(void)
{
	char path[PATH_SIZE];
	DIR *mdir;
	struct dirent *dent;
	size_t i;
	int r, pr;

	for (i = 0; i < clip.count; i++) {
	    if (clip.entries[i].line) free(clip.entries[i].line);
	    clip.entries[i].fname = 0;
	}
	clip.count = 0;

	if ((mdir = opendir(maindir)) == NULL)
		return 1;

	r = pr = 0;
	while ((dent = readdir(mdir))) {
		if (!sscanf(dent->d_name, "E%d", &r)) continue;
		if (r == pr) goto skip;
		pr = r;

		if (clip.count < clip.capacity) {
			clip.entries[clip.count].fname = r;
			clip.entries[clip.count].line = NULL;
			clip.count++;
			continue;
		}

		if (clip.entries[0].line) free(clip.entries[0].line);
		snprintf(path, PATH_SIZE, "%s/E%d", maindir, clip.entries[0].fname);
		if (!(flags & FLAG_KEEP)) {
				if (remove(path) < 0) return 1;
		}
		clip.entries[0].fname = r;
		clip.entries[0].line = NULL;

		sortentries();
skip:
		r = 0;
	}
	closedir(mdir);

	for (i = 0; i < clip.count; i++) {
		snprintf(path, PATH_SIZE, "%s/E%d", maindir, clip.entries[i].fname);
		clip.entries[i].line = getlinepreview(path);
	}

	return 0;
}

int
storentry(char *text)
{
	FILE *f;
	char path[PATH_SIZE];

	if (strnlen(text, LINE_SIZE) < 2) return 1;
	if (duplicate_text(text)) return 1;

	snprintf(path, PATH_SIZE, "%s/E%d", maindir, (int)time(NULL));
	if ((f = fopen(path, "w")) == NULL)
		die("fopen(): %s '%s'", path, strerror(errno));
	fputs(text, f);
	fclose(f);

	if (chmod(path, FILEMASK) < 0)
		die("chmod(): %s '%s'", path, strerror(errno));

	return 0;
}

void
usage(char *program)
{
	fprintf(stderr, "usage: %s [-d directory] [-v] [-V] [-1] [-k]\n", program);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *clipboard;
	XFixesSelectionNotifyEvent *sel_event;
	XEvent event;
	int i, fd;
	char path[PATH_SIZE];

	flags = 0;
	maindir = NULL;
	if (!(ctx.dpy = XOpenDisplay(NULL)))
		die("can't open X display: %s", strerror(errno));
	ctx.root = DefaultRootWindow(ctx.dpy);
	ctx.clipboard = XInternAtom(ctx.dpy, "CLIPBOARD", False);
	ctx.primary = XInternAtom(ctx.dpy, "PRIMARY", False);
	ctx.win = XCreateSimpleWindow(ctx.dpy, ctx.root,
	    1, 1, 1, 1, 1, CopyFromParent, CopyFromParent);

#ifdef __OpenBSD__
	if (pledge("stdio fattr rpath wpath cpath flock unveil", NULL) < 0)
		die("pledge(): %s", strerror(errno));
#endif

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d")) {
			maindir = argv[++i];
			if (access(maindir, F_OK) < 0)
				die("access(): %s '%s'", path, strerror(errno));
#ifdef __OpenBSD__
			if (unveil(maindir, "rwc") < 0)
				die("unveil(): %s '%s'", path, strerror(errno));
#endif
		} else if (!strcmp(argv[i], "-v")) {
			puts("scm-" VERSION " © 2024 Thim Cederlund");
			puts("https://github.com/thimc/scm");
			exit(0);
		} else if (!strcmp(argv[i], "-k")) {
			flags |= FLAG_KEEP;
		} else {
			usage(argv[0]);
		}
	}

	if (!maindir) {
		usage(argv[0]);
		exit(1);
	}

	snprintf(path, PATH_SIZE, "%s/lock", maindir);
	if ((fd = open(path, O_RDWR | O_CREAT, FILEMASK)) < 0)
		die("open(): %s '%s'", path, strerror(errno));
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		if (errno == EWOULDBLOCK) {
			die("%s is already running!", argv[0]);
		} else {
			die("flock(): %s '%s'", path, strerror(errno));
		}
	}

	if (XFixesQueryExtension(ctx.dpy, &ctx.event_base, &ctx.error_base) == 0)
		die("xfixes(): '%s'", strerror(errno));
	XFixesSelectSelectionInput(ctx.dpy, ctx.root, ctx.clipboard,
			XFixesSetSelectionOwnerNotifyMask);

	clip.count = 0;
	clip.capacity = MAXENTRIES;
	clip.entries = ecalloc(clip.capacity, sizeof(*clip.entries));

	do {
		if (!scanentries()) makelinecache();
		XNextEvent(ctx.dpy, &event);
		if (event.type == (ctx.event_base + XFixesSelectionNotify)) {
			sel_event = (XFixesSelectionNotifyEvent*)&event;
			if (sel_event->selection == ctx.clipboard) {
				if ((clipboard = get_utf_prop(ctx, ctx.clipboard)))
					if (!storentry(clipboard))
						free(clipboard);
				continue;
			}
		}
		sleep(1);
	} while (1);

	for (i = 0; i < (int)clip.capacity; i++)
		free(clip.entries[i].line);
	free(clip.entries);

	if (flock(fd, LOCK_UN) < 0)
		die("flock(): %s '%s'", path, strerror(errno));
	XDestroyWindow(ctx.dpy, ctx.win);
	XCloseDisplay(ctx.dpy);

	return 0;
}
