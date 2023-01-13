# scm - simple clipboard manager

scm is a minimalist clipboard manager manager for X.

This project is a work in progress and is expected to crash if used\
for extended amounts of time, see the TODO section below for future goals.

## Requirements
In order to build scm you need the Xlib and Xfixes header files.\
It is also expected that you have dmenu(1) installed.

## Running scm
By default, scm does nothing when you run it. The executable takes\
the following arguments:

-   -d specifies the working directory for scm
-   -v spits out the version number and exits
-   -V prints debug messages to stderr
-   -1 runs scm for one loop iteration (fetch, save and quit)
-   -p listens for PRIMARY selection changes

It is recommended for new users to run scm via the supplied `scmd` shell\
script because it sets everything up for the user.

The scmd script sets the working directory to \$XDG_CACHE_HOME or\
\$HOME/.cache/scm and all arguments passed to scmd are passed to scm.

Once scmd has been started, run `scmenu` to get a dmenu prompt.\
By default scm will store up to 30 entries but this behavior can\
be changed by editing the config.h file and recompiling scm.


## TODO
[ ] (Probably?) Fix a bunch of memory leaks
[x] Make sure that duplicate clipboard entries aren't stored
[x] Add a flag that allows scm to listen for PRIMARY clipboard changes.

