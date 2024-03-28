PROG = scm
VERSION = 0.1

INCS = config.h util.h
SRCS = $(PROG).c util.c
OBJS = $(SRCS:.c=.o)

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/man

CFLAGS  += -Wall -Wextra\
		   `pkg-config --cflags x11,xfixes`\
		   -DVERSION=\"${VERSION}\"
LDFLAGS += `pkg-config --libs x11,xfixes`

all: options $(PROG)

options:
	@echo "$(PROG) build options:"
	@echo "PREFIX   = $(PREFIX)"
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"

.c.o:
	$(CC) -c $(CFLAGS) $<

$(OBJS): $(INCS)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -f $(PROG) $(OBJS)

install: $(PROG)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(PROG) scmd scmenu $(DESTDIR)$(PREFIX)/bin/
	sed "s/VERSION/${VERSION}/g" < $(PROG).1 > ${DESTDIR}${MANPREFIX}/man1/$(PROG).1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROG)\
		$(DESTDIR)$(PREFIX)/bin/scmd\
		$(DESTDIR)$(PREFIX)/bin/scmenu\
		${DESTDIR}${MANPREFIX}/man1/$(PROG).1

.PHONY: all options clean install uninstall
