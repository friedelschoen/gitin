.POSIX:

NAME = stagit
VERSION = 1.2

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/man
DOCPREFIX = ${PREFIX}/share/doc/${NAME}

LIBGIT_INC = -I/usr/local/include
LIBGIT_LIB = -L/usr/local/lib -lgit2

# use system flags.
STAGIT_CFLAGS = ${LIBGIT_INC} ${CFLAGS}
STAGIT_LDFLAGS = ${LIBGIT_LIB} ${LDFLAGS}
STAGIT_CPPFLAGS = -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -D_BSD_SOURCE

# Uncomment to enable workaround for older libgit2 which don't support this
# option. This workaround will be removed in the future *pinky promise*.
#STAGIT_CFLAGS += -DGIT_OPT_SET_OWNER_VALIDATION=-1

SRC = \
	stagit.c
COMPATSRC = \
	compat.c
BIN = \
	stagit
MAN1 = \
	stagit.1
DOC = \
	LICENSE\
	README
HDR = compat.h

COMPATOBJ = \
	compat.o

OBJ = ${SRC:.c=.o} ${COMPATOBJ}

all: ${BIN}

.o:
	${CC} -o $@ ${LDFLAGS}

.c.o:
	${CC} -o $@ -c $< ${STAGIT_CFLAGS} ${STAGIT_CPPFLAGS}

dist:
	rm -rf ${NAME}-${VERSION}
	mkdir -p ${NAME}-${VERSION}
	cp -f ${MAN1} ${HDR} ${SRC} ${COMPATSRC} ${DOC} \
		Makefile favicon.png logo.png style.css \
		example_create.sh example_post-receive.sh \
		${NAME}-${VERSION}
	# make tarball
	tar -cf - ${NAME}-${VERSION} | \
		gzip -c > ${NAME}-${VERSION}.tar.gz
	rm -rf ${NAME}-${VERSION}

${OBJ}: ${HDR}

stagit: stagit.o ${COMPATOBJ}
	${CC} -o $@ stagit.o ${COMPATOBJ} ${STAGIT_LDFLAGS}

clean:
	rm -f ${BIN} ${OBJ} ${NAME}-${VERSION}.tar.gz

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

.PHONY: all clean dist install uninstall
