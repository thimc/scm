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
#include "config.h"
#include "util.h"

void
debug(const char* fmt, ...)
{
	va_list ap;

	if (!verbose)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

void
die(const char* fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

void*
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("%s: %s\n", __FUNCTION__, strerror(errno));
	return p;
}

char*
get_utf_prop(xorg instance, const char* bufname, const char* fmtname)
{
	char* result = NULL;
	char* out = NULL;
	unsigned long ressize, restail;
	int resbits;
	XEvent ev;
	Atom bufid  = XInternAtom(instance.display, bufname, False);
	Atom fmtid  = XInternAtom(instance.display, fmtname, False);
	Atom propid = XInternAtom(instance.display, "XSEL_DATA", False);
	Atom incrid = XInternAtom(instance.display, "INCR", False);

	XConvertSelection(instance.display, bufid, fmtid, propid,
			instance.window, CurrentTime);
	do {
		XNextEvent(instance.display, &ev);
	} while (ev.type != SelectionNotify || ev.xselection.selection != bufid);

	if (ev.xselection.property) {
		XGetWindowProperty(instance.display, instance.window, propid,
				0, LONG_MAX / 4, False, AnyPropertyType, &fmtid, &resbits,
				&ressize, &restail, (unsigned char**)&result);
		if (fmtid == incrid)
			die("%s: buffer too large. INCR reading isn't implemented\n",
					__FUNCTION__, strerror(errno));
		out = strndup(result, (int)ressize);
		XFree(result);
	}
	return out;
}

