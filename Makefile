NAME = gitin
VERSION = 0.1

# paths
PREFIX = /usr/local

# library flags
LIBS = libgit2 libarchive

# use system flags.
CC ?= gcc
CFLAGS := -Wall -Wextra -Wpedantic -Werror -O2 -g -D_XOPEN_SOURCE=700 $(shell pkg-config --cflags $(LIBS))
LDFLAGS := $(shell pkg-config --libs $(LIBS))

BIN = ${NAME}

MAN1 = ${NAME}.1

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
	makearchive.h \
	murmur3.h \
	parseconfig.h \
	writer.h

OBJECTS = \
	commit.o \
	common.o \
	compat.o \
	config.o \
	gitin.o \
	hprintf.o \
	makearchive.o \
	murmur3.o \
	parseconfig.o \
	writefiles.o \
	writehtml.o \
	writeindex.o \
	writelog.o \
	writerepo.o

all: ${BIN} ${MAN1} compile_flags.txt

%.o: %.c ${HEADER}
	${CC} -c -o $@ $< ${CFLAGS} ${CPPFLAGS}

${BIN}: ${OBJECTS}
	${CC} -o $@ $^ ${LDFLAGS}

%: %.in
	sed 's/%VERSION%/${VERSION}/g' $< > $@

.PHONY: all clean install uninstall dist

clean:
	rm -f ${BIN} ${OBJECTS} ${MAN1} compile_flags.txt

compile_flags.txt:
	echo ${CFLAGS} ${CPPFLAGS} | tr ' ' '\n' > $@

install: all
	mkdir -p ${PREFIX}/bin
	for f in ${BIN}; do \
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
	for f in ${BIN}; do \
		rm -f ${PREFIX}/bin/$$f; \
	done
	for f in ${MAN1}; do \
		rm -f ${PREFIX}/share/man/man1/$$f; \
	done
	rm -rf ${PREFIX}/share/doc/${NAME}