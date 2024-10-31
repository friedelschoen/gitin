NAME = gitin
VERSION = 0.1
HOMEPAGE = https://git.friedelschoen.io/web/gitin

# paths
PREFIX ?= /usr/local

# flags
CC 		 ?= gcc
CFLAGS 	 += -Wall -Wextra -Wpedantic -Werror \
		    $(shell pkg-config --cflags $(LIBS))
CPPFLAGS += -D_XOPEN_SOURCE=700 -D_GNU_SOURCE -DVERSION=\"$(VERSION)\" -DGIT_DEPRECATE_HARD
LDFLAGS  += $(shell pkg-config --libs $(LIBS))

ifeq ($(CC),gcc)
CFLAGS += -Wno-stringop-truncation \
		  -Wno-format-truncation
endif

ifneq ($(DEBUG),)
CFLAGS += -g -O0
CPPFLAGS += -DDEBUG
else
CFLAGS += -O2
endif

BINS = gitin gitin-findrepos gitin-configtree
MAN1 = gitin.1 gitin-findrepos.1
MAN5 = gitin.conf.5

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
	arg.h \
	buffer.h \
	common.h \
	config.h \
	execute.h \
	hprintf.h \
	macro.h \
	parseconfig.h \
	path.h \
	writer.h

OBJECTS = \
	buffer.o \
	common.o \
	config.o \
	execute.o \
	filetypes.o \
	getdiff.o \
	getrefs.o \
	hprintf.o \
	parseconfig.o \
	path.o \
	writearchive.o \
	writeatom.o \
	writecommit.o \
	writefiles.o \
	writehtml.o \
	writeindex.o \
	writejson.o \
	writelog.o \
	writepreview.o \
	writeredirect.o \
	writerefs.o \
	writerepo.o \
	writeshortlog.o

.PHONY: all clean \
	install install-bins install-man1 install-man5 install-assets install-icons \
	uninstall uninstall-bins uninstall-man1 uninstall-man5 uninstall-assets

# default target, make everything
all: $(BINS) $(MAN1) $(MAN5) compile_flags.txt

# automatic tagets

%.o: %.c $(HEADER)
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

%: %.in Makefile
	sed 's/%VERSION%/$(VERSION)/g;s|%HOMEPAGE%|$(HOMEPAGE)|g' $< > $@

%: %.o
	$(CC) -o $@ $^ $(LDFLAGS)

# binary targets

gitin: LIBS = libgit2 libarchive
gitin: $(OBJECTS)

gitin-findrepos: LIBS = libgit2
gitin-findrepos: gitin-findrepos.o config.o

gitin-configtree:
gitin-configtree: gitin-configtree.py
	install -m755 $^ $@

compile_flags.txt: LIBS = libgit2 libarchive
compile_flags.txt: Makefile
	echo $(CFLAGS) $(CPPFLAGS) | tr ' ' '\n' > $@

filetypes.c: filetypes.txt
	@echo "const char* filetypes[][3] = {" > $@
	sed -E 's/([^ ]+) ([^ ]+)( ([^ ]*))?/{ "\1", "\2", "\4" },/' $< >> $@
	@echo "{0} };" >> $@

# pseudo targets

clean:
	rm -f $(BINS) $(BINS:=.o) $(OBJECTS) $(MAN1) $(MAN5) compile_flags.txt filetypes.c

install: install-bins install-man1 install-man5 install-assets install-icons

install-bins: $(BINS)
	install -d $(PREFIX)/bin
	install -m 755 $(BINS) $(PREFIX)/bin

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
	rm -f $(addprefix $(PREFIX)/bin/, $(BINS))

uninstall-man1:
	rm -f $(addprefix $(PREFIX)/share/man/man1/, $(MAN1))

uninstall-man5:
	rm -f $(addprefix $(PREFIX)/share/man/man5/, $(MAN5))

uninstall-assets:
	rm -rf $(PREFIX)/share/doc/$(NAME)
