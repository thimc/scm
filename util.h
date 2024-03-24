#define PATH_SIZE			50
#define LINE_SIZE			80
#define LINE_COUNTER_SIZE	15

typedef struct {
	Display *dpy;
	Window win, root;
	Atom clipboard, primary, utf8;
	int event_base, error_base;
} context;

typedef struct {
	char *line;
	int fname;
} entry;

typedef struct {
	entry *entries;
	size_t count, capacity;
} entries;

void die(const char *fmt,...);
void *ecalloc(size_t nmemb, size_t size);
char *get_utf_prop(context ctx, Atom atom);
