/* how many clipboard entries scm will keep track of */
#define MAXENTRIES	30

/* file mask for all entries, see chmod(2) */
#define FILEMASK S_IRUSR | S_IWUSR

/* the working directory that contains the clipboard entries, set via -d flag */
static char *maindir;

extern int flags;

#define FLAG_KEEP	 (1 << 1)
