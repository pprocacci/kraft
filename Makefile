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
INSTALL_PROGRAM?=	install -m 555
INSTALL_MAN?=	install -m 444

VERSION!=	cat VERSION
LIBSRCS!=	echo src/*.c
HDRS!=		echo include/*.h
LIBOBJS=	${LIBSRCS:.c=.o}
TESTS=		t/json t/sse

all: kraft

kraft: kraft.o libkraft.a
	${CC} ${LDFLAGS} -o kraft kraft.o libkraft.a

libkraft.a: ${LIBOBJS}
	rm -f libkraft.a
	${AR} rcs libkraft.a ${LIBOBJS}

${LIBOBJS} kraft.o: ${HDRS}

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

t/json: t/json.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/json t/json.c libkraft.a

t/sse: t/sse.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/sse t/sse.c libkraft.a

test: ${TESTS}
	./t/json
	./t/sse

install: kraft
	mkdir -p ${DESTDIR}${BINDIR} ${DESTDIR}${MANDIR}/man1
	${INSTALL_PROGRAM} kraft ${DESTDIR}${BINDIR}/kraft
	${INSTALL_MAN} man/kraft.1 ${DESTDIR}${MANDIR}/man1/kraft.1

dist:
	git archive --prefix=kraft-${VERSION}/ -o kraft-${VERSION}.tar.gz HEAD

clean:
	rm -f kraft kraft.o libkraft.a ${LIBOBJS} ${TESTS} kraft-*.tar.gz

.PHONY: all test install dist clean
