NAME = gitin
VERSION = 0.1
HOMEPAGE = https://git.friedelschoen.io/web/gitin

# paths
PREFIX ?= /usr/local

# flags
CC 		 ?= gcc
CFLAGS 	 += -std=c99 -Wall -Wextra -Wpedantic -Werror -Wno-format-truncation -Wno-unknown-warning-option \
		    $(if $(LIBS),$(shell pkg-config --cflags $(LIBS)),)
CPPFLAGS += -D_XOPEN_SOURCE=700 -D_GNU_SOURCE -DVERSION=\"$(VERSION)\" -DGIT_DEPRECATE_HARD -Iinclude
LDFLAGS  += $(if $(LIBS),$(shell pkg-config --libs $(LIBS)),)

ifneq ($(DEBUG),)
CFLAGS += -g -O0
CPPFLAGS += -DDEBUG
else
CFLAGS += -O2
endif

BINS = \
	gitin \
	gitin-index \
	gitin-configtree \
	gitin-findrepos

INSTBINS = \
	gitin \
	gitin-index \
	gitin-configtree \
	gitin-findrepos

MAN1 = \
	gitin.1 \
	gitin-configtree.1 \
	gitin-findrepos.1

MAN5 = \
	gitin.conf.5

DOCS = \
	assets/favicon.png \
	assets/logo.png \
	assets/style.css \
	README.md \
	LICENSE

ICONS = \
	icons/image.svg \
	icons/binary.svg \
	icons/source.svg \
	icons/text.svg \
	icons/readme.svg \
	icons/directory.svg \
	icons/other.svg \
	icons/command.svg

HEADER = \
	include/arg.h \
	include/buffer.h \
	include/common.h \
	include/composer.h \
	include/config.h \
	include/execute.h \
	include/findrepo.h \
	include/getinfo.h \
	include/hprintf.h \
	include/macro.h \
	include/parseconfig.h \
	include/path.h \
	include/writer.h

OBJECTS = \
	lib/buffer.o \
	lib/common.o \
	lib/composefiletree.o \
	lib/composerepo.o \
	lib/config.o \
	lib/execute.o \
	lib/filetypes.o \
	lib/findrepo.o \
	lib/getdiff.o \
	lib/getindex.o \
	lib/getrefs.o \
	lib/getrepo.o \
	lib/hprintf.o \
	lib/parseconfig.o \
	lib/path.o \
	lib/writearchive.o \
	lib/writeatom.o \
	lib/writecommit.o \
	lib/writehtml.o \
	lib/writeindex.o \
	lib/writejson.o \
	lib/writelog.o \
	lib/writepreview.o \
	lib/writeredirect.o \
	lib/writerefs.o \
	lib/writeshortlog.o \
	lib/writesummary.o

CLEAN = \
	$(BINS) \
	$(MAN1) \
	$(MAN5) \
	*.o \
	lib/*.o \
	lib/filetypes.c

DEV = \
	compile_flags.txt \

.PHONY: all clean clean-all \
	install install-bins install-man1 install-man5 install-assets install-icons \
	uninstall uninstall-bins uninstall-man1 uninstall-man5 uninstall-assets staticcheck

# default target, make everything
all: $(BINS) $(MAN1) $(MAN5) $(DEV)

# automatic tagets

%.o: %.c $(HEADER)
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

%: %.md
	sed 's/%VERSION%/$(VERSION)/g;s|%HOMEPAGE%|$(HOMEPAGE)|g' $< | lowdown -s -tman -o $@

%: %.o
	$(CC) -o $@ $^ $(LDFLAGS)

# targets

gitin: LIBS = libgit2 libarchive
gitin: $(OBJECTS)

gitin-index: LIBS = libgit2 libarchive
gitin-index: $(OBJECTS)

gitin-configtree: gitin-configtree.py
	install -m755 $^ $@

gitin-findrepos: LIBS = libgit2
gitin-findrepos: lib/config.o lib/findrepo.o lib/hprintf.o lib/path.o

compile_flags.txt: LIBS = libgit2 libarchive
compile_flags.txt: Makefile
	echo $(CFLAGS) $(CPPFLAGS) | tr ' ' '\n' > $@

lib/filetypes.c: filetypes.txt
	awk 'BEGIN { print "const char* filetypes[][3] = {" } \
	/^#/ { next } \
	{ printf("{ \"%s\", \"%s\", \"%s\" },\n", $$1, $$2, $$3) } \
	END { print "{0} };" }' $< > $@

# pseudo targets

clean:
	rm -f $(CLEAN)

clean-all:
	rm -f $(CLEAN) $(DEV)

install: install-bins install-man1 install-man5 install-assets install-icons

install-bins: $(INSTBINS)
	install -d $(PREFIX)/bin
	install -m 755 $(INSTBINS) $(PREFIX)/bin

install-man1: $(MAN1)
	install -d $(PREFIX)/share/man/man1
	install -m 644 $(MAN1) $(PREFIX)/share/man/man1

install-man5: $(MAN5)
	install -d $(PREFIX)/share/man/man5
	install -m 644 $(MAN5) $(PREFIX)/share/man/man5

install-assets: $(DOCS)
	install -d $(PREFIX)/share/doc/$(NAME)
	install -m 644 $(DOCS) $(PREFIX)/share/doc/$(NAME)

install-icons: $(ICONS)
	install -d $(PREFIX)/share/doc/$(NAME)/icons
	install -m 644 $(ICONS) $(PREFIX)/share/doc/$(NAME)/icons

uninstall: uninstall-bins uninstall-man1 uninstall-man5 uninstall-assets

uninstall-bins:
	rm -f $(addprefix $(PREFIX)/bin/, $(INSTBINS))

uninstall-man1:
	rm -f $(addprefix $(PREFIX)/share/man/man1/, $(MAN1))

uninstall-man5:
	rm -f $(addprefix $(PREFIX)/share/man/man5/, $(MAN5))

uninstall-assets:
	rm -rf $(PREFIX)/share/doc/$(NAME)

staticcheck:
	cppcheck --enable=all --std=c99 -Iinclude/ --cppcheck-build-dir=.cppcheck --suppress=missingIncludeSystem --check-level=exhaustive .