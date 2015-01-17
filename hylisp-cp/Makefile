SRC      = 
INC	 = 
OBJ	 = $(SRC:%.c=%.o)
EXE      = hylispcp

#
#	misc junk
#
RCS      = RCS
MISC     = Makefile README 

#
#	compile/load options
#

CC	 = clang
CFLAGS   = -O2 -L/usr/local/lib/ -lexpat -L. -DHAVE_IPV6 -Wall -lpthread
LDLIBS   = 
LDFLAGS  = 

LISP_H = /usr/src/sys/net/lisp/lisp.h

${EXE}: 
	${CC}    radix/*_*.c server.c  db.c udp.c hmac/*.c cli.c list/list.c thr_pool/*.c parser.c plumbing.c -DOPENLISP plugin_openlisp.c -DVIRTUAL_SUPPORT plugin_hv/plugin_hv.c -o ${EXE} -g  -O2  -I/usr/local/include  -L/usr/local/lib -lexpat -L. -DHAVE_IPV6 -Wall -lpthread ; \

install:
	/bin/cp ${EXE} /usr/sbin/
	/bin/chmod 755 /usr/sbin/${EXE}
	/bin/mkdir -p /etc/hylispcp
	/bin/cp -f conf/* /etc/hylispcp/

clean:
	/bin/rm -f ${OBJ} ${EXE} Make.log Make.err *~
