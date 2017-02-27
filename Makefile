PREFIX	=/opt
BINDIR	=${PREFIX}/bin

CCMODE	=32
CC	=gcc -m${CCMODE}
CFLAGS	=-Wall -Werror -pedantic -std=gnu99 -D_FORTIFY_SOURCE=2
LDFLAGS	=

all:	slowcat

check:	slowcat

clean:
	${RM} *.o a.out core.* lint tags

distclean clobber: clean
	${RM} slowcat

install: slowcat
	install -d ${BINDIR}
	install -c -s slowcat ${BINDIR}/

uninstall:
	${RM} ${BINDIR}/slowcat
