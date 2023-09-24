CC = cc
NAME = scm
VERSION = 0.1
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man
INC = config.h util.h
SRC = $(NAME).c util.c
OBJ = $(SRC:.c=.o)

CFLAGS  += -g -Wall -Wextra\
		   `pkg-config --cflags x11,xfixes`\
		   -DVERSION=\"${VERSION}\"
LDFLAGS += `pkg-config --libs x11,xfixes`

all: options $(NAME)

options:
	@echo "$(NAME) build options:"
	@echo "PREFIX   = $(PREFIX)"
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"

.c.o:
	$(CC) -c $(CFLAGS) $<

$(OBJ): $(INC)

$(NAME): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(NAME) $(OBJ)

install: $(NAME)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(NAME) scmd scmenu $(DESTDIR)$(PREFIX)/bin/
	sed "s/VERSION/${VERSION}/g" < scm.1 > ${DESTDIR}${MANPREFIX}/man1/scm.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(NAME)\
		$(DESTDIR)$(PREFIX)/bin/scmd\
		$(DESTDIR)$(PREFIX)/bin/scmenu\
		${DESTDIR}${MANPREFIX}/man1/scm.1

.PHONY: all options clean install uninstall
