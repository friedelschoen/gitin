NAME = gitin
VERSION = 0.1

# paths
PREFIX = /usr/local

# flags
CC 		 ?= gcc
CFLAGS 	 += -Wall -Wextra -Wpedantic -Werror -O2 -g \
		    $(shell pkg-config --cflags $(LIBS))
CPPFLAGS += -D_XOPEN_SOURCE=700 -D_GNU_SOURCE -DVERSION=\"$(VERSION)\" -DGIT_DEPRECATE_HARD
LDFLAGS  += $(shell pkg-config --libs $(LIBS))

BINS = gitin findrepos
MAN1 = gitin.1 findrepos.1

DOCS = \
	assets/favicon.png \
	assets/logo.png \
	assets/style.css

HEADER = \
	arg.h \
	filetypes.h \
	gitin.h

OBJECTS = \
	common.o \
	config.o \
	hprintf.o \
	getdiff.o \
	parseconfig.o \
	writearchive.o \
	writeatom.o \
	writecommit.o \
	writefiles.o \
	writehtml.o \
	writeindex.o \
	writejson.o \
	writelog.o \
	writerefs.o \
	writerepo.o

.PHONY: all clean install uninstall

# default target, make everything
all: $(BINS) $(MAN1) compile_flags.txt

# automatic tagets

%.o: %.c $(HEADER)
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

%: %.in
	sed 's/%VERSION%/$(VERSION)/g' $< > $@

%: %.o
	$(CC) -o $@ $^ $(LDFLAGS)

# binary targets

gitin: LIBS = libgit2 libarchive
gitin: $(OBJECTS)

findrepos: LIBS = libgit2
findrepos: findrepos.o config.o

compile_flags.txt: LIBS = libgit2 libarchive
compile_flags.txt: Makefile
	echo $(CFLAGS) $(CPPFLAGS) | tr ' ' '\n' > $@

filetypes.h: filetypes.txt
	@echo "static const char* filetypes[][2] = {" > $@
	sed -E 's/([a-z]+) (.*)/{ "\2", "\1" },/' $< >> $@
	@echo "{0} };" >> $@

# pseudo targets

clean:
	rm -f $(BINS) $(BINS:=.o) $(OBJECTS) $(MAN1) compile_flags.txt filetypes.h

install: $(BINS) $(MAN1)
	install -d $(PREFIX)/bin
	install -m 755 $(BINS) $(PREFIX)/bin

	install -d $(PREFIX)/share/man/man1
	install -m 644 $(MAN1) $(PREFIX)/share/man/man1

	install -d $(PREFIX)/share/doc/$(NAME)
	install -m 644 $(DOCS) $(PREFIX)/share/doc/$(NAME)

uninstall:
	rm -f $(addprefix $(PREFIX)/bin/, $(BINS))
	rm -f $(addprefix $(PREFIX)/share/man/man1/, $(MAN1))
	rm -rf $(PREFIX)/share/doc/$(NAME)
