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

int duplicate(const char *text);
int getentries(void);
int cmp(const void *a, const void *b);
int getlinecount(const char *path);
char* getfirstline(const char *path);
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
	return ((const entry*)a)->fname > ((const entry*)b)->fname;
}

void
makelinecache(void)
{
	char path[EPATHSIZE], cpath[EPATHSIZE];
	FILE *f;
	int i;

	snprintf(cpath, EPATHSIZE, "%s/line_cache", maindir);
	debug("generating line cche to %s", cpath);
	if ((f = fopen(cpath, "w")) == NULL)
		die("%s: fopen: %s '%s'", __func__, path, strerror(errno));

	qsort(clipentries, MAXENTRIES, sizeof(entry), cmp);
	for (i = 0; i < MAXENTRIES; i++) {
		snprintf(path, EPATHSIZE, "%s/E%d", maindir, clipentries[i].fname);
		if (access(path, F_OK) < 0)
			continue;
		fprintf(f, "%s\t%s%s\n", path, clipentries[i].line,
				clipentries[i].counter);
	}
	fclose(f);
}

char*
getlinecounter(const char* path)
{
	FILE *f;
	char *buf;
	int chr;
	int nlines = 0;

	if ((f = fopen(path, "r")) == NULL)
		die("%s: fopen: %s '%s'", __func__, path, strerror(errno));
	buf = ecalloc(ECNTRSIZE, sizeof(char));
	while ((chr = fgetc(f)) != EOF)
		nlines += (chr=='\n');
	fclose(f);

	if (nlines > 1)
		snprintf(buf, ECNTRSIZE, " (%d lines)", nlines);

	return buf;
}

char*
getfirstline(const char* path)
{
	FILE *f;
	char *buf;

	if ((f = fopen(path, "r")) == NULL)
		die("%s: fopen: %s '%s'", __func__, path, strerror(errno));
	buf = ecalloc(ELINESIZE, sizeof(char));
	if ((fgets(buf, ELINESIZE, f)) == NULL)
		die("%s: fgets: %s '%s'", __func__, path, strerror(errno));
	fclose(f);

	return buf;
}

void
readentries(void)
{
	int i;
	char path[EPATHSIZE];

	for (i = 0; i < MAXENTRIES; i++) {
		if(!clipentries[i].fname) continue;
		snprintf(path, EPATHSIZE, "%s/E%d", maindir, clipentries[i].fname);
		if (access(path, F_OK) < 0)
			die("%s: access: %s '%s'", __func__, path, strerror(errno));
		clipentries[i].line = getfirstline(path);
		clipentries[i].counter = getlinecounter(path);
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
			qsort(clipentries, (size_t)f, sizeof(entry), cmp);
			if (r > clipentries[0].fname) {
				snprintf(path, EPATHSIZE, "%s/E%d", maindir, clipentries[0].fname);
				if (remove(path) < 0)
					die("%s: remove: '%s' %s", __func__, path, strerror(errno));

				clipentries[0].fname = r;

			} else {
				snprintf(path, EPATHSIZE, "%s/E%d", maindir, r);
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

	snprintf(path, EPATHSIZE, "%s/E%d", maindir, (int)time(NULL));
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
	XEvent event;
	int i, fd;
	char path[EPATHSIZE];

	flags = 0;
	maindir = NULL;
	if (!(instance.display = XOpenDisplay(NULL)))
		die("%s: can't open X display: %s", __func__, strerror(errno));
	instance.root = DefaultRootWindow(instance.display);
	instance.clipboard = XInternAtom(instance.display, "CLIPBOARD", False);
	instance.primary = XInternAtom(instance.display, "PRIMARY", False);
	instance.window = XCreateSimpleWindow(instance.display, instance.root,
			 1, 1, 1, 1, 1, CopyFromParent, CopyFromParent);

#ifdef __OpenBSD__
	if (pledge("stdio fattr rpath wpath cpath flock unveil", NULL) < 0)
		die("%s: pledge: %s", __func__, strerror(errno));
#endif

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d")) { /* set main directory */
			maindir = argv[++i];
			debug("main dir set to %s", maindir);
			if (access(maindir, F_OK) < 0)
				die("%s: access %s '%s'", __func__, path, strerror(errno));
#ifdef __OpenBSD__
			if (unveil(maindir, "rwc") < 0)
				die("%s: unveil %s '%s'", __func__, path, strerror(errno));
#endif
		} else if (!strcmp(argv[i], "-p")) { /* PRIMARY changes */
			flags |= FLAG_PRIMARY;
		} else if (!strcmp(argv[i], "-v")) { /* prints version information */
			puts("scm-" VERSION" © 2023 Thim Cederlund");
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
	if ((fd = open(path, O_RDWR|O_CREAT, FILEMASK)) < 0)
		die("%s: open: %s '%s'", __func__, path, strerror(errno));

	if (flock(fd, LOCK_EX  | LOCK_NB) < 0) {
		if (errno == EWOULDBLOCK)
			die("%s is already running!", argv[0]);
		else
			die("%s: flock: %s '%s'", __func__, path, strerror(errno));
	}

	if (XFixesQueryExtension(instance.display, &instance.event_base,
				&instance.error_base) == 0) {
		die("%s: xfixes '%s'", __func__, strerror(errno));
	}

	XFixesSelectSelectionInput(instance.display, instance.root,
			instance.clipboard, XFixesSetSelectionOwnerNotifyMask);

	if (flags & FLAG_PRIMARY) {
		XFixesSelectSelectionInput(instance.display, instance.root,
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
		XNextEvent(instance.display, &event);
		if (event.type == (instance.event_base + XFixesSelectionNotify)) {
			if (((XFixesSelectionNotifyEvent *) & event)->selection
			== instance.clipboard) {
				if ((clipboard = get_utf_prop(instance, "CLIPBOARD"))) {
					if (!storeclip(clipboard))
						goto skip;
				}
			} else if (((XFixesSelectionNotifyEvent *) & event)->selection
					== instance.primary && (flags & FLAG_PRIMARY)) {
				if ((primary = get_utf_prop(instance, "PRIMARY"))) {
					if(storeclip(primary) < 0)
						goto skip;
				}
			}

		}
skip:
		fflush(stdout);
		sleep(1); /* TODO: to avoid filename clashing */
	} while (!(flags & FLAG_ONESHOT));

	for (i = 0; i < MAXENTRIES; i++) {
		free(clipentries[i].line);
		free(clipentries[i].counter);
	}
	free(clipentries);
	free(clipboard);

	if (flock(fd, LOCK_UN) < 0)
		die("%s: flock: %s '%s'", __func__, path, strerror(errno));

	XDestroyWindow(instance.display, instance.window);
	XCloseDisplay(instance.display);

	return 0;
}
