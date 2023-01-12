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

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include "util.h"
#include "config.h"

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
	DEBUG("generating line cche to %s\n", cpath);
	if ((f = fopen(cpath, "w")) == NULL)
		die("%s: fopen: %s '%s'", __FUNCTION__, path, strerror(errno));

	/* DEBUG("printing clip entries\n"); */
	for (i = 0; i < MAXENTRIES; i++) {
		snprintf(path, EPATHSIZE, "%s/E%d", maindir, clipentries[i].fname);
		if (access(path, F_OK) != 0)
			continue;
		fprintf(f, "%s\t%s%s\n", path, clipentries[i].line,
				clipentries[i].counter);
		/* DEBUG("%3d: %s\t%s%s\n", i, path, clipentries[i].line, */
		/* 		clipentries[i].counter); */
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
		die("%s: fopen: %s '%s'", __FUNCTION__, path, strerror(errno));
	buf = calloc(ECNTRSIZE, sizeof(char));
	while ((chr = fgetc(f)) != EOF)
		c += (chr=='\n');
	fclose(f);
	/* DEBUG("lines found: %d\n", c); */
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
		die("%s: fopen: %s '%s'", __FUNCTION__, path, strerror(errno));
	buf = malloc(ELINESIZE * sizeof(char));
	if ((fgets(buf, ELINESIZE, f)) == NULL)
		die("%s: fgets: %s '%s'", __FUNCTION__, path, strerror(errno));
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
		if (access(path, F_OK) != 0)
			continue;
			/* TODO: OH NO, um, something isn't quite right here! Future me
			 * will probably fix it. if it ain't broke don't fix it :) */
			/* die("%s: access: %s '%s'", __FUNCTION__, path, strerror(errno)); */
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
		die("%s: opendir: %s '%s'", __FUNCTION__, maindir, strerror(errno));
	filenames = calloc(fcounter, sizeof(int));
	f = 0;
	while ((dent = readdir(mdir)) != NULL) {
		if (!sscanf(dent->d_name, "E%d", &r))
			continue;
		DEBUG("readdir %3d: %s\n", f, dent->d_name);

		filenames[f] = r;

		f++;
		if (f >= fcounter) {
			filenames = realloc(filenames, (fcounter + MAXENTRIES) * sizeof(int));
			if (filenames == NULL)
				die("%s: realloc: '%s'", __FUNCTION__, strerror(errno));
			fcounter += MAXENTRIES;
			DEBUG("realloc'd %d\n", fcounter);
		}
	}
	closedir(mdir);

	DEBUG("readdir %d files, have allocated %d. max %d slots\n",
			f, fcounter, MAXENTRIES);

	qsort(filenames, fcounter, sizeof(int), cmp);
	DEBUG("sorted!\n");
	o = MAXENTRIES;

	for(i = fcounter - 1; i >= 0; i--) {
		if (filenames[i] <= 0)
			continue;
		if (o >= 0) {
			DEBUG("store val: %d (%d) (%d slots available)\n",
					filenames[i], i, o);
			clipentries[MAXENTRIES - o].fname = filenames[i];
			o--;
		} else {
			snprintf(path, EPATHSIZE, "%s/E%d", maindir, filenames[i]);
			if (remove(path) != 0)
				die("%s: remove: '%s' %s", __FUNCTION__, path, strerror(errno));
			DEBUG("removed excess entry: %s\n", path);
		}
	}

	free(filenames);
}

void
storeclip(const char *text)
{
	FILE *f;
	char path[EPATHSIZE];
	char* clip = (char*)text;

	snprintf(path, EPATHSIZE, "%s/E%d", maindir, (int)time(NULL));
	DEBUG("fetched '%s' from clipboard! storing to %s\n", clip, path);

	if ((f = fopen(path, "w")) == NULL)
		errx(1, "storeclip fopen: '%s': %s", path, strerror(errno));
	fprintf(f, "%s", text);
	fclose(f);

	if (chmod(path, FILEMASK) != 0)
		DEBUG("storeclip chmod '%s': %s", path, strerror(errno));
}

void
usage(void)
{
	fprintf(stderr, "usage: scm [-d directory] [-v] [-V] [-1]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *clipboard;
	XEvent event;
	int i;

	instance.display = XOpenDisplay(NULL);
	if (instance.display == NULL) {
		errx(1, "can't open X display: %s", strerror(errno));
	}
	instance.root = DefaultRootWindow(instance.display);
	instance.clipboard = XInternAtom(instance.display, "CLIPBOARD", False);
	instance.window = XCreateSimpleWindow(instance.display, instance.root,
			 1, 1, 1, 1, 1, CopyFromParent, CopyFromParent);

#ifdef __OpenBSD__
	if (pledge("stdio fattr rpath wpath cpath unveil", NULL) != 0) {
		errx(1, "pledge: %s", strerror(errno));
	}
#endif

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d")) { /* set main directory */
			maindir = argv[++i];
			DEBUG("main dir set to %s", maindir);
#ifdef __OpenBSD__
			if (unveil(maindir, "rwc") != 0)
				errx(1, "unveil: %s", strerror(errno));
			if (access(maindir, F_OK) != 0)
				errx(1, "directory '%s': %s", maindir, strerror(errno));
#endif
		} else if (!strcmp(argv[i], "-v")) { /* prints version information */
			puts("scm-" VERSION);
			exit(0);
		} else if (!strcmp(argv[i], "-V")) {
			verbose = 1;
		} else if (!strcmp(argv[i], "-1")) {
			oneshot = 1;
		} else {
			usage();
		}
	}

	if (maindir == NULL)
		exit(1);

	if (XFixesQueryExtension(instance.display, &instance.event_base,
				&instance.error_base) == 0)
		errx(1, "xfixes query extension: %s", strerror(errno));

	XFixesSelectSelectionInput(instance.display, instance.root,
		     instance.clipboard, XFixesSetSelectionOwnerNotifyMask);

	clipentries = ecalloc(MAXENTRIES, sizeof(entry));

	do {
		/* printf("\e[1;1H\e[2J"); */

		scanentries();
		readentries();
		makelinecache();

		for (i = 0; i < MAXENTRIES; i++)
			DEBUG("%3d: [%d] %s%s\n", i, clipentries[i].fname,
				clipentries[i].line, clipentries[i].counter);

		DEBUG("Waiting for clipboard changes..");
		XNextEvent(instance.display, &event);
		if (event.type == (instance.event_base + XFixesSelectionNotify) &&
				((XFixesSelectionNotifyEvent *) & event)->selection
				== instance.clipboard) {
			if ((clipboard = get_utf_prop(instance,
							"CLIPBOARD", "UTF8_STRING")) != NULL) {
				/* TODO: check for duplicates */
				storeclip(clipboard);
			}
		}
		fflush(stdout);
		sleep(1); /* TODO: Find a better work around. for now we have
					 to sleep in order make sure that the clip entry
					 we just stored isn't overwritten by another clip
					 entry that was copied during the exact same second. */
	} while (!oneshot);

	free(clipboard);
	for (i = 0; i < MAXENTRIES + 1; i++) {
		free(clipentries[i].line);
		free(clipentries[i].counter);
	}
	free(clipentries);

	XDestroyWindow(instance.display, instance.window);
	XCloseDisplay(instance.display);
	return 0;
}

