/* See LICENSE file for copyright and license details. */
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include "util.h"
#include "config.h"

int duplicate(const char* text);
int getentries(void);
int cmp(const void* a, const void* b);
int getlinecount(const char *path);
char* getfirstline(const char *path);
void makelinecache(void);
void trimlist(void);
void sortentries(void);
void scanentries(void);
void storeclip(const char *text);
void usage(void);

int verbose = 0; /* defined as extern in config.h */
entry *clipentries;
static xorg instance;

int
duplicate(const char* text)
{
	int i;

	for (i = 0; i < MAXENTRIES; i++) {
		if (clipentries[i].line == NULL)
			continue;
		if (strcmp(clipentries[i].line, text) == 0) {
			debug("duplicate entry: '%s' matched index %d", text, i);
			return 1;
		}
	}
	return 0;
}

int
cmp(const void* a, const void* b)
{
	return *(const int*)a - *(const int*)b;
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

	for (i = 0; i < MAXENTRIES; i++) {
		snprintf(path, EPATHSIZE, "%s/E%d", maindir, clipentries[i].fname);
		if (access(path, F_OK) < 0)
			continue;
		fprintf(f, "%s\t%s%s\n", path, clipentries[i].line, clipentries[i].counter);
	}
	fclose(f);
}

char*
getlinecounter(const char* path)
{
	FILE* f;
	char* buf;
	char chr;
	int c = 0;

	if ((f = fopen(path, "r")) == NULL)
		die("%s: fopen: %s '%s'", __func__, path, strerror(errno));
	buf = calloc(ECNTRSIZE, sizeof(char));
	while ((chr = fgetc(f)) != EOF)
		c += (chr=='\n');
	fclose(f);
	/* debug("lines found: %d", c); */
	if (c > 1)
		snprintf(buf, ECNTRSIZE, " (%d lines)", c);
	return buf;
}

char*
getfirstline(const char* path)
{
	FILE* f;
	char* buf;

	if ((f = fopen(path, "r")) == NULL)
		die("%s: fopen: %s '%s'", __func__, path, strerror(errno));
	buf = malloc(ELINESIZE * sizeof(char));
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
		snprintf(path, EPATHSIZE, "%s/E%d", maindir, clipentries[i].fname);
		if (access(path, F_OK) < 0)
			continue;
			/* TODO: OH NO, um, something isn't quite right here! Future me
			 * will probably fix it. if it ain't broke don't fix it :) */
			/* die("%s: access: %s '%s'", __func__, path, strerror(errno)); */
		clipentries[i].line = getfirstline(path);
		clipentries[i].counter = getlinecounter(path);
	}
}

void
scanentries(void)
{
	int f, r, i, o;
	int fcounter = MAXENTRIES;
	int* filenames;
	DIR* mdir;
	struct dirent* dent;
	char path[EPATHSIZE];

	if ((mdir = opendir(maindir)) == NULL)
		die("%s: opendir: %s '%s'", __func__, maindir, strerror(errno));
	filenames = calloc(fcounter, sizeof(int));
	f = 0;
	while ((dent = readdir(mdir)) != NULL) {
		if (!sscanf(dent->d_name, "E%d", &r))
			continue;
		debug("readdir %.3d: %s", f, dent->d_name);

		filenames[f] = r;

		f++;
		if (f >= fcounter) {
			filenames = realloc(filenames, (fcounter + MAXENTRIES) * sizeof(int));
			if (filenames == NULL)
				die("%s: realloc: '%s'", __func__, strerror(errno));
			fcounter += MAXENTRIES;
			debug("realloc'd %d", fcounter);
		}
	}
	closedir(mdir);

	debug("readdir %d files, have allocated %d. max %d slots",
			f, fcounter, MAXENTRIES);

	qsort(filenames, fcounter, sizeof(int), cmp);
	o = MAXENTRIES;

	for(i = fcounter - 1; i >= 0; i--) {
		if (filenames[i] <= 0)
			continue;
		if (o >= 0) {
			debug("store val: %d (%d) (%d slots available)",
					filenames[i], i, o);
			clipentries[MAXENTRIES - o].fname = filenames[i];
			o--;
		} else {
			snprintf(path, EPATHSIZE, "%s/E%d", maindir, filenames[i]);
			if (remove(path) < 0)
				die("%s: remove: '%s' %s", __func__, path, strerror(errno));
			debug("removed excess entry: %s", path);
		}
	}

	free(filenames);
}

void
storeclip(const char *text)
{
	FILE *f;
	char path[EPATHSIZE];

	if (duplicate(text))
		return;

	snprintf(path, EPATHSIZE, "%s/E%d", maindir, (int)time(NULL));
	debug("fetched '%s' from clipboard! storing to %s", text, path);

	if ((f = fopen(path, "w")) == NULL)
		die("%s: fopen %s '%s'", __func__, path, strerror(errno));
	fprintf(f, "%s", text);
	fclose(f);

	if (chmod(path, FILEMASK) < 0)
		die("%s: chmod %s '%s'", __func__, path, strerror(errno));
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
	int primaryflag = 0, oneshot = 0;
	char path[EPATHSIZE];

	instance.display = XOpenDisplay(NULL);
	if (instance.display == NULL) {
		die("%s: can't open X display: %s", __func__, strerror(errno));
	}
	instance.root = DefaultRootWindow(instance.display);
	instance.clipboard = XInternAtom(instance.display, "CLIPBOARD", False);
	instance.primary = XInternAtom(instance.display, "PRIMARY", False);
	instance.window = XCreateSimpleWindow(instance.display, instance.root,
			 1, 1, 1, 1, 1, CopyFromParent, CopyFromParent);

#ifdef __OpenBSD__
	if (pledge("stdio fattr rpath wpath cpath flock unveil", NULL) < 0) {
		die("%s: pledge: %s", __func__, strerror(errno));
	}
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
			primaryflag++;
		} else if (!strcmp(argv[i], "-v")) { /* prints version information */
			puts("scm-" VERSION);
			exit(0);
		} else if (!strcmp(argv[i], "-V")) {
			verbose++;
		} else if (!strcmp(argv[i], "-1")) {
			oneshot++;
		} else {
			usage();
		}
	}

	if (maindir == NULL)
		exit(1);

	snprintf(path, EPATHSIZE, "%s/lock", maindir);
	debug("got a session lock for '%s'", path);
	if ((fd = open(path, O_RDWR|O_CREAT, FILEMASK)) < 0)
		die("%s: open: %s '%s'", __func__, path, strerror(errno));

	if (flock(fd, LOCK_EX|LOCK_NB) < 0) {
		if (errno == EWOULDBLOCK)
			die("%s is already running!", argv[0]);
		else
			die("%s: flock: %s '%s'", __func__, path, strerror(errno));
	}

	if (XFixesQueryExtension(instance.display, &instance.event_base,
				&instance.error_base) == 0)
		die("%s: xfixes '%s'", __func__, strerror(errno));

	XFixesSelectSelectionInput(instance.display, instance.root,
		     instance.clipboard, XFixesSetSelectionOwnerNotifyMask);

	if (primaryflag)
		XFixesSelectSelectionInput(instance.display, instance.root,
		     instance.primary, XFixesSetSelectionOwnerNotifyMask);

	clipentries = ecalloc(MAXENTRIES, sizeof(entry));

	do {
		scanentries();
		readentries();
		makelinecache();

		for (i = 0; i < MAXENTRIES; i++)
			if (clipentries[i].line)
				debug("%.3d: [%d] %s%s", i, clipentries[i].fname,
					clipentries[i].line, clipentries[i].counter);

		debug("Waiting for clipboard changes..");
		XNextEvent(instance.display, &event);
		if (event.type == (instance.event_base + XFixesSelectionNotify)) {
			if (((XFixesSelectionNotifyEvent *) & event)->selection
					== instance.clipboard) {
				if ((clipboard = get_utf_prop(instance, "CLIPBOARD",
								"UTF8_STRING")) != NULL) {
					storeclip(clipboard);
				}
			} else if (((XFixesSelectionNotifyEvent *) & event)->selection
					== instance.primary && primaryflag) {
				if ((primary = get_utf_prop(instance, "PRIMARY",
								"UTF8_STRING")) != NULL) {
					storeclip(primary);
				}
			}
		}
		fflush(stdout);
		sleep(1); /* TODO: Find a better work around. for now we have
					 to sleep in order make sure that the clip entry
					 we just stored isn't overwritten by another clip
					 entry that was copied during the exact same second. */
	} while (!oneshot);

	for (i = 0; i < MAXENTRIES + 1; i++) {
		free(clipentries[i].line);
		free(clipentries[i].counter);
	}
	free(clipentries);
	free(clipboard);

	if (flock(fd, LOCK_UN) < 0)
		die("%s: flock LOCK_UN: %s '%s'", __func__, path, strerror(errno));

	XDestroyWindow(instance.display, instance.window);
	XCloseDisplay(instance.display);
	return 0;
}

