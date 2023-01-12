/* See LICENSE file for copyright and license details. */

/* how many entries scm will keep track of in history */
#define MAXENTRIES 30

/* file mask for all clip entries, see chmod(1) */
#define FILEMASK S_IRUSR | S_IWUSR

/* directory containing all clipboard entries, set via -d flag */
static char* maindir = NULL;

/*
 * ENAME: each clip entry is stored with the file name E + UNIX time stamp
 * ECNTR: how many characters for the additional string "(X lines)"
 * EPATH: the complete path to where the entry is located, usually a
 *        combination of XDG_HOME_CACHE and ENAME.
 * ELINE: how many characters we allow for each line preview
 */
#define ENAMESIZE 20
#define ECNTRSIZE 20
#define EPATHSIZE 50
#define ELINESIZE 80

/* if the debug level is greater then 0 will print debug info to STDOUT */
extern int verbose;

/* if > 0 then it will run the program one loop iteration, set via -1 flag */
static int oneshot = 0;

