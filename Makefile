PREFIX     ?= /usr
BINPREFIX  ?= $(PREFIX)/bin
MANPREFIX  ?= $(PREFIX)/share/man
CONFPREFIX ?= $(PREFIX)/share/pegasus
DOCPREFIX  ?= $(PREFIX)/share/doc

RM           ?= rm -f
INSTALL_DIR  ?= install -m755 -d
INSTALL_PROG ?= install -m755
INSTALL_FILE ?= install -m644

CFLAGS   += -I. -O2 `pkg-config --cflags x11`
LIBS     += `pkg-config --libs x11`
CPPFLAGS += -DVERSION=\"$(VERSION)\" -DSYSCONFPATH=\"$(CONFPREFIX)\"

ifdef DEBUG
CFLAGS  += -g3 -ggdb3 -std=c99 -pedantic -Og $(INCS) -DDEBUG $(DEFS)
LDFLAGS += -g3 -ggdb3
endif

VERSION = 0.4.9


PIXMAPS = close.xbm iconify.xbm max.xbm 
SRC     = draw.c pegasus.c ewmh.c parse.c
HEADERS = config.h pegasus.h
OBJ     = $(SRC:.c=.o)

all: pegasus $(HEADERS)

.c.o:
	@echo CC $<
	$(CC) $(CPPFLAGS) -c $(CFLAGS) $<

$(OBJ): $(HEADERS)

pegasus: $(OBJ) $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

clean:
	$(RM) pegasus $(OBJ) pegasus-$(VERSION).tar.gz *~

install:
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/share/pegasus
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/share/man/man1
	$(INSTALL_PROG) pegasus $(DESTDIR)$(PREFIX)/bin/pegasus
	$(INSTALL_FILE) config/* $(DESTDIR)$(PREFIX)/share/pegasus
	sed -i -e "s/VERSION/$(VERSION)/g;s|CONFDIR|$(DESTDIR)$(PREFIX)/share/pegasus|g" pegasus.1
	$(INSTALL_FILE) pegasus.1 $(DESTDIR)$(PREFIX)/share/man/man1/pegasus.1

uninstall:
	$(RM) $(DESTDIR)$(BINPREFIX)/bin/pegasus
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/pegasus.1
	$(RM) -r $(DESTDIR)$(CONFPREFIX)
