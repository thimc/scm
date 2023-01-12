/* See LICENSE file for copyright and license details. */

typedef struct xorg_t xorg;
typedef struct entry_t entry;

struct xorg_t {
	Display* display;
	Window window, root;
	Atom clipboard, utf8;
	int event_base, error_base;
};

struct entry_t {
	int fname;
	char* line;
	char* counter;
};

void debug(const char* fmt, ...);
void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
char* get_utf_prop(xorg instance, const char* bufname, const char* fmtname);

