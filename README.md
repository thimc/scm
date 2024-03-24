# scm - simple clipboard manager

scm is a minimalist clipboard manager for X.

## Requirements
In order to build scm you need the Xlib and Xfixes header files.
It is also expected that you have [dmenu](https://tools.suckless.org/dmenu/) installed.

Simply run `make` and then (as root) `make install` to install scm.

## Running scm

The user is required to set a cache directory for scm (via the `-d`
flag), this directory will store all clipboard entries that scm
gets so it is important to set the permissions for the directory
accordingly.

In this repository you will find a `scmd` script that sets up a cache
directory in the users `$HOME/.cache` directory.

To interact with scm, a dmenu wrapper script (`scmenu`) is provided.
The script allows the user to select and copy the entries in to
their clipboard.

See the `scm(1)` manpage for more information.

## License
ISC
