/* See LICENSE file for copyright and license details. */
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/extensions/Xfixes.h>
#include "X11/Xlib.h"
#include "config.h"
#include "util.h"

void
debug(const char *fmt,...)
{
	va_list ap;

	if (!(flags & FLAG_VERBOSE))
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

void
die(const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("%s: %s\n", __func__, strerror(errno));
	return p;
}

char *
get_utf_prop(xorg instance, Atom atom)
{
	char *result = NULL;
	char *out = NULL;
	size_t ressize, restail;
	int resbits;
	XEvent ev;
	Atom fmtid = XInternAtom(instance.dpy, "UTF8_STRING", False);
	Atom propid = XInternAtom(instance.dpy, "XSEL_DATA", False);
	Atom incrid = XInternAtom(instance.dpy, "INCR", False);

	XConvertSelection(instance.dpy, atom, fmtid, propid, instance.win,
	    CurrentTime);
	do {
		XNextEvent(instance.dpy, &ev);
	} while (ev.type != SelectionNotify || ev.xselection.selection != atom);

	if (ev.xselection.property) {
		XGetWindowProperty(instance.dpy, instance.win, propid,
		    0, LONG_MAX / 4, False, AnyPropertyType, &fmtid, &resbits,
		    &ressize, &restail, (unsigned char **) &result);
		if (fmtid == incrid) {
			die("%s: buffer too large. INCR reading isn't implemented\n",
			    __func__, strerror(errno));
		}
		out = strndup(result, ressize);
		XFree(result);
	}
	return out;
}
