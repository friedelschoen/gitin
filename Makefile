.POSIX:

NAME = stagit
VERSION = 1.2

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/man
DOCPREFIX = ${PREFIX}/share/doc/${NAME}

# use system flags.
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2 $(shell pkg-config --cflags libgit2)
LDFLAGS = $(shell pkg-config --libs libgit2)
CPPFLAGS = -D_XOPEN_SOURCE=700 

BIN = stagit

MAN1 = stagit.1

HEADER = \
	arg.h \
	commitinfo.h \
	common.h \
	compat.h \
	config.h \
	deltainfo.h \
	murmur3.h \
	refinfo.h \
	xml.h

OBJECTS = \
	commitinfo.o \
	common.o \
	compat.o \
	deltainfo.o \
	murmur3.o \
	refinfo.o \
	stagit.o \
	xml.o

all: ${BIN}

%.o: %.c ${HEADER}
	${CC} -c -o $@ $< ${CFLAGS} ${CPPFLAGS}

stagit: ${OBJECTS}
	${CC} -o $@ $^ ${LDFLAGS}

clean:
	rm -f ${BIN} ${OBJECTS}

install: all
	# installing executable files.
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${BIN} ${DESTDIR}${PREFIX}/bin
	for f in ${BIN}; do chmod 755 ${DESTDIR}${PREFIX}/bin/$$f; done
	cp -f stagit-highlight ${DESTDIR}${PREFIX}/bin/
	# installing example files.
	mkdir -p ${DESTDIR}${DOCPREFIX}
	cp -f style.css\
		favicon.png\
		logo.png\
		example_create.sh\
		example_post-receive.sh\
		README\
		${DESTDIR}${DOCPREFIX}
	# installing manual pages.
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f ${MAN1} ${DESTDIR}${MANPREFIX}/man1
	for m in ${MAN1}; do chmod 644 ${DESTDIR}${MANPREFIX}/man1/$$m; done

uninstall:
	# removing executable files.
	for f in ${BIN}; do rm -f ${DESTDIR}${PREFIX}/bin/$$f; done
	# removing example files.
	rm -f \
		${DESTDIR}${DOCPREFIX}/style.css\
		${DESTDIR}${DOCPREFIX}/favicon.png\
		${DESTDIR}${DOCPREFIX}/logo.png\
		${DESTDIR}${DOCPREFIX}/example_create.sh\
		${DESTDIR}${DOCPREFIX}/example_post-receive.sh\
		${DESTDIR}${DOCPREFIX}/README
	-rmdir ${DESTDIR}${DOCPREFIX}
	# removing manual pages.
	for m in ${MAN1}; do rm -f ${DESTDIR}${MANPREFIX}/man1/$$m; done

.PHONY: all clean install uninstall
