/* See LICENSE file for copyright and license details. */

#define PATH_SIZE			50
#define LINE_SIZE			80
#define LINE_COUNTER_SIZE	15

typedef struct {
	Display *dpy;
	Window win, root;
	Atom clipboard, primary, utf8;
	int event_base, error_base;
} xorg;

typedef struct {
	char *line;
	int fname;
} entry;

typedef struct {
	entry **entries;
	size_t capacity;
	size_t len;
} entries;

void debug(const char *fmt,...);
void die(const char *fmt,...);
void *ecalloc(size_t nmemb, size_t size);
char *get_utf_prop(xorg instance, Atom atom);
