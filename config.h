/* See LICENSE file for copyright and license details. */

/* how many entries scm will keep track of */
#define MAXENTRIES 10

/* file mask for all clip entries, see chmod(1) */
#define FILEMASK S_IRUSR | S_IWUSR

/* if any clipboard activity comes from a title matched below it'll be ignored */
static const char *ignore_rules[] = {
	".*— Mozilla Firefox$",
};

/* directory containing all clipboard entries, set via -d flag */
static char *maindir;

#define FLAG_PRIMARY (1 << 0)
#define FLAG_VERBOSE (1 << 1)
#define FLAG_ONESHOT (1 << 2)
extern int flags;
