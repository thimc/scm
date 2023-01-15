# scm - simple clipboard manager

scm is a minimalist clipboard manager manager for X.

*NOTE: This project is a work in progress and is expected to crash*

## Requirements
In order to build scm you need the Xlib and Xfixes header files.\
It is also expected that you have dmenu(1) installed.

## Running scm

Arguments:
* `-d` specifies the cache directory
* `-v` prints the version number
* `-V` enables debug messages which are printed to stderr
* `-1` runs scm for one loop iteration (fetch, save and quit)
* `-p` listens for PRIMARY selection changes

The `scmd` script sets the cache directory to **\$XDG_CACHE_HOME** or\
**\$HOME/.cache/scm**. All arguments passed to scmd are redirected to scm.

The `scmenu` script parses the `line_cache` file generated by scm and pipes\
the data to `dmenu`. By default scm will store up to 30 entries.\
This behavior can be changed by editing the config.h file and recompiling scm.

## TODO
Please keep in mind that the ideas below are not guaranteed to be implemented.
- [ ] add a ruleset for ignoring clipboard activity based on the window title
that are defined in a list (think password managers)
- [ ] add a flag which allows for syncing the contents between PRIMARY and clipboard
- [x] Make sure that duplicate clipboard entries aren't stored
- [x] Add a flag that allows scm to listen for PRIMARY clipboard changes.

