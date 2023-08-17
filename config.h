/* See LICENSE file for copyright and license details. */

/* how many entries scm will keep track of */
#define MAXENTRIES 30

/* file mask for all clip entries, see chmod(1) */
#define FILEMASK S_IRUSR | S_IWUSR

/* directory containing all clipboard entries, set via -d flag */
static char *maindir;

/* ENAME: each clip entry is stored with the file name E + UNIX time stamp
 * ECNTR: how many characters for the additional string "(X lines)"
 * EPATH: the complete path to where the entry is located, usually a
 *        combination of XDG_HOME_CACHE and ENAME.
 * ELINE: how many characters we allow for each line preview
 */
#define ENAMESIZE 20
#define ECNTRSIZE 20
#define EPATHSIZE 50
#define ELINESIZE 80

#define FLAG_PRIMARY (1 << 0)
#define FLAG_VERBOSE (1 << 1)
#define FLAG_ONESHOT (1 << 2)
extern int flags;
