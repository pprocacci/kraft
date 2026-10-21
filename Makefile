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
AGENTSRCS!=	echo agent/*.c
AGENTOBJS=	${AGENTSRCS:.c=.o}
TESTS=		t/json t/sse t/conv t/perm t/mcp t/mcpd t/fakeapi

all: kraft

kraft: kraft.o ${AGENTOBJS} libkraft.a
	${CC} ${LDFLAGS} -o kraft kraft.o ${AGENTOBJS} libkraft.a ${LDLIBS}

libkraft.a: ${LIBOBJS}
	rm -f libkraft.a
	${AR} rcs libkraft.a ${LIBOBJS}

${LIBOBJS} ${AGENTOBJS} kraft.o: ${HDRS}
${AGENTOBJS} kraft.o: agent/agent.h

kraft.o: kraft.c VERSION
	${CC} ${CFLAGS} -DKRAFT_VERSION=\"${VERSION}\" -c kraft.c -o kraft.o

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

t/json: t/json.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/json t/json.c libkraft.a ${LDLIBS}

t/sse: t/sse.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/sse t/sse.c libkraft.a ${LDLIBS}

t/conv: t/conv.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/conv t/conv.c libkraft.a ${LDLIBS}

t/perm: t/perm.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/perm t/perm.c libkraft.a ${LDLIBS}

t/mcp: t/mcp.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/mcp t/mcp.c libkraft.a ${LDLIBS}

t/mcpd: t/mcpd.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/mcpd t/mcpd.c libkraft.a ${LDLIBS}

t/fakeapi: t/fakeapi.c libkraft.a
	${CC} ${CFLAGS} ${LDFLAGS} -o t/fakeapi t/fakeapi.c libkraft.a ${LDLIBS}

test: kraft ${TESTS}
	./t/json
	./t/sse
	./t/conv
	./t/perm
	./t/mcp
	sh t/agent.sh

# the fuzz harness: libFuzzer, with the address and undefined
# behaviour sanitizers, built from the library's sources
FUZZFLAGS?=	-g -O1 -fsanitize=fuzzer,address,undefined \
		-fno-sanitize-recover=undefined
fuzz: t/fuzz
t/fuzz: t/fuzz.c ${LIBSRCS} ${HDRS}
	${CC} ${FUZZFLAGS} -Iinclude -o t/fuzz t/fuzz.c ${LIBSRCS} ${LDLIBS}

install: kraft
	mkdir -p ${DESTDIR}${BINDIR} ${DESTDIR}${MANDIR}/man1
	${INSTALL_PROGRAM} kraft ${DESTDIR}${BINDIR}/kraft
	${INSTALL_MAN} man/kraft.1 ${DESTDIR}${MANDIR}/man1/kraft.1

dist:
	git archive --prefix=kraft-${VERSION}/ -o kraft-${VERSION}.tar.gz HEAD

clean:
	rm -f kraft kraft.o libkraft.a ${LIBOBJS} ${AGENTOBJS} ${TESTS} \
	    kraft-*.tar.gz

.PHONY: all test fuzz install dist clean
