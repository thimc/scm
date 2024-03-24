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
get_utf_prop(context ctx, Atom atom)
{
	char *result = NULL;
	char *out = NULL;
	size_t ressize, restail;
	int resbits;
	XEvent ev;
	Atom fmtid = XInternAtom(ctx.dpy, "UTF8_STRING", False);
	Atom propid = XInternAtom(ctx.dpy, "XSEL_DATA", False);
	Atom incrid = XInternAtom(ctx.dpy, "INCR", False);

	XConvertSelection(ctx.dpy, atom, fmtid, propid, ctx.win, CurrentTime);
	do {
		XNextEvent(ctx.dpy, &ev);
	} while (ev.type != SelectionNotify || ev.xselection.selection != atom);

	if (ev.xselection.property) {
		XGetWindowProperty(ctx.dpy, ctx.win, propid,
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
