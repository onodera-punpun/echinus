PREFIX     ?= /usr
BINPREFIX  ?= $(PREFIX)/bin
MANPREFIX  ?= $(PREFIX)/share/man
CONFPREFIX ?= $(PREFIX)/share/echinus
DOCPREFIX  ?= $(PREFIX)/share/doc

RM           ?= rm -f
INSTALL_DIR  ?= install -m755 -d
INSTALL_PROG ?= install -m755
INSTALL_FILE ?= install -m644

CFLAGS   += -I. -O2 `pkg-config --cflags x11 xft`
LIBS     += `pkg-config --libs x11 xft`
CPPFLAGS += -DVERSION=\"$(VERSION)\" -DSYSCONFPATH=\"$(CONFPREFIX)\"

ifdef DEBUG
CFLAGS  += -g3 -ggdb3 -std=c99 -pedantic -Og $(INCS) -DDEBUG $(DEFS)
LDFLAGS += -g3 -ggdb3
endif

VERSION = 0.4.9


PIXMAPS = close.xbm iconify.xbm max.xbm 
SRC     = draw.c echinus.c ewmh.c parse.c
HEADERS = config.h echinus.h
OBJ     = $(SRC:.c=.o)

all: echinus $(HEADERS)

.c.o:
	@echo CC $<
	$(CC) $(CPPFLAGS) -c $(CFLAGS) $<

$(OBJ): $(HEADERS)

echinus: $(OBJ) $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

clean:
	$(RM) echinus $(OBJ) echinus-$(VERSION).tar.gz *~

install:
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/share/echinus
	$(INSTALL_DIR) $(DESTDIR)$(PREFIX)/share/man/man1
	$(INSTALL_PROG) echinus $(DESTDIR)$(PREFIX)/bin/echinus
	$(INSTALL_FILE) echinusrc $(DESTDIR)$(PREFIX)/share/echinus/echinusrc
	for file in $(PIXMAPS); do \
		$(INSTALL_FILE) $${file} $(DESTDIR)$(PREFIX)/share/echinus/$${file} ; \
	done ;
	sed -i -e "s/VERSION/$(VERSION)/g;s|CONFDIR|$(DESTDIR)$(PREFIX)/share/echinus|g" echinus.1
	$(INSTALL_FILE) echinus.1 $(DESTDIR)$(PREFIX)/share/man/man1/echinus.1

uninstall:
	$(RM) $(DESTDIR)$(BINPREFIX)/bin/echinus
	$(RM) $(DESTDIR)$(MANPREFIX)/man1/echinus.1
	$(RM) -r $(DESTDIR)$(CONFPREFIX)
