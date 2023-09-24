/* See LICENSE file for copyright and license details. */

/* how many clipboard entries scm will keep track of */
#define MAXENTRIES	10

/* file mask for all entries, see chmod(1) */
#define FILEMASK	S_IRUSR | S_IWUSR

/* the working directory that contains the clipboard entries, set via -d flag */
static char *maindir = NULL;

#define FLAG_PRIMARY	(1 << 0)
#define FLAG_VERBOSE	(1 << 1)
#define FLAG_ONESHOT	(1 << 2)
#define FLAG_KEEP		(1 << 3)
extern int flags;

// vim:ts=4 tw=4 cc=80
