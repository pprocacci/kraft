# kraft: see HACKING for the rules of the road.
# Written for FreeBSD make(1); honours CC, CFLAGS, LDFLAGS, PREFIX
# and DESTDIR so that the port can drive it.

PREFIX?=	/usr/local
BINDIR?=	${PREFIX}/bin
MANDIR?=	${PREFIX}/share/man
CC?=		cc
AR?=		ar
CFLAGS?=	-O2 -pipe
CFLAGS+=	-Wall -Wextra -Iinclude
SSLLIBS?=	-lssl -lcrypto
LDLIBS?=	-ledit ${SSLLIBS}
INSTALL_PROGRAM?=	install -m 555
INSTALL_MAN?=	install -m 444

VERSION!=	cat VERSION
LIBSRCS!=	echo src/*.c
HDRS!=		echo include/*.h
LIBOBJS=	${LIBSRCS:.c=.o}
TESTS=		t/json t/sse t/conv

all: kraft

kraft: kraft.o libkraft.a
	${CC} ${LDFLAGS} -o kraft kraft.o libkraft.a ${LDLIBS}

libkraft.a: ${LIBOBJS}
	rm -f libkraft.a
	${AR} rcs libkraft.a ${LIBOBJS}

${LIBOBJS} kraft.o: ${HDRS}

kraft.o: kraft.c VERSION
	${CC} ${CFLAGS} -DKRAFT_VERSION=\"${VERSION}\" -c kraft.c -o kraft.o

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

t/json: t/json.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/json t/json.c libkraft.a

t/sse: t/sse.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/sse t/sse.c libkraft.a

t/conv: t/conv.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/conv t/conv.c libkraft.a

test: ${TESTS}
	./t/json
	./t/sse
	./t/conv

install: kraft
	mkdir -p ${DESTDIR}${BINDIR} ${DESTDIR}${MANDIR}/man1
	${INSTALL_PROGRAM} kraft ${DESTDIR}${BINDIR}/kraft
	${INSTALL_MAN} man/kraft.1 ${DESTDIR}${MANDIR}/man1/kraft.1

dist:
	git archive --prefix=kraft-${VERSION}/ -o kraft-${VERSION}.tar.gz HEAD

clean:
	rm -f kraft kraft.o libkraft.a ${LIBOBJS} ${TESTS} kraft-*.tar.gz

.PHONY: all test install dist clean
