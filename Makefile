NAME = gitin
VERSION = 0.1

# paths
PREFIX = /usr/local

# use system flags.
CC ?= gcc
CFLAGS = -Wall -Wextra -Wpedantic -Werror -O2 -g -D_XOPEN_SOURCE=700 $(shell pkg-config --cflags $(LIBS)) 
LDFLAGS = $(shell pkg-config --libs $(LIBS))

BINS = gitin findrepos

MAN1 = gitin.1 findrepos.1

DOCS = \
	favicon.png \
	logo.png \
	style.css \

HEADER = \
	arg.h \
	commit.h \
	common.h \
	compat.h \
	config.h \
	hprintf.h \
	murmur3.h \
	parseconfig.h \
	writer.h

OBJECTS = \
	commit.o \
	common.o \
	compat.o \
	config.o \
	hprintf.o \
	murmur3.o \
	parseconfig.o \
	writearchive.o \
	writefiles.o \
	writehtml.o \
	writeindex.o \
	writelog.o \
	writerefs.o \
	writerepo.o

all: ${BINS} ${MAN1} compile_flags.txt

%.o: %.c ${HEADER}
	${CC} -c -o $@ $< ${CFLAGS} ${CPPFLAGS}

%: %.in
	sed 's/%VERSION%/${VERSION}/g' $< > $@

%: %.o
	${CC} -o $@ $^ ${LDFLAGS}

gitin: LIBS = libgit2 libarchive
gitin: ${OBJECTS}

findrepos: LIBS = libgit2
findrepos: findrepos.o

.PHONY: all clean install uninstall dist

clean:
	rm -f ${BINS} ${BINS:=.o} ${OBJECTS} ${MAN1} compile_flags.txt

compile_flags.txt: LIBS=libgit2 libarchive
compile_flags.txt:
	echo ${CFLAGS} ${CPPFLAGS} | tr ' ' '\n' > $@

install: all
	mkdir -p ${PREFIX}/bin
	for f in ${BINS}; do \
		install -Dm 755 $$f ${PREFIX}/bin/$$f; \
	done

	mkdir -p ${PREFIX}/share/man/man1
	for f in ${MAN1}; do \
		install -Dm 644 $$f ${PREFIX}/share/man/man1/$$f; \
	done

	mkdir -p  ${PREFIX}/share/doc
	for f in ${DOCS}; do \
		install -Dm 644 assets/$$f ${PREFIX}/share/doc/${NAME}/$$f; \
	done

uninstall:
	for f in ${BINS}; do \
		rm -f ${PREFIX}/bin/$$f; \
	done
	for f in ${MAN1}; do \
		rm -f ${PREFIX}/share/man/man1/$$f; \
	done
	rm -rf ${PREFIX}/share/doc/${NAME}