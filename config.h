/* See LICENSE file for copyright and license details. */

/* how many entries scm will keep track of */
#define MAXENTRIES 30

/* file mask for all clip entries, see chmod(1) */
#define FILEMASK S_IRUSR | S_IWUSR

/* directory containing all clipboard entries, set via -d flag */
static char *maindir;

#define FLAG_PRIMARY (1 << 0)
#define FLAG_VERBOSE (1 << 1)
#define FLAG_ONESHOT (1 << 2)
extern int flags;
