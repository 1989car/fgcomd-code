SVNDEF=-D'SVN_REV="$(shell svnversion -n .)"'
#CFLAGS=-D__DEBUG__ -g -L/usr/lib/debug -I/usr/lib/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/gnet-2.0/include -I/usr/include/gnet-2.0
CFLAGS=-g -L/usr/lib/debug -I/usr/lib/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/gnet-2.0/include -I/usr/include/gnet-2.0
#CFLAGS=-I/usr/lib/glib-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/gnet-2.0/include -I/usr/include/gnet-2.0
LIBS=-lgnet-2.0 -lglib-2.0 -lm -pthread -lgthread-2.0 -lrt

all: fgcomd lighthouse listener

indent: *.c *.h
	indent \
		--use-tabs \
		--tab-size5 \
		--indent-level5 \
		--blank-lines-after-declarations \
		--blank-lines-after-procedures \
		--blank-lines-before-block-comments \
		--braces-after-if-line \
		--brace-indent0 \
	$?

print: *.[hc]
	trueprint \
		--ps-level-two \
		--no-page-break-after-function \
		--intelligent-line-wrap \
		--tabsize=5 \
		--double-sided \
		--output=source-code.ps \
	 $?
	ps2pdf14 source-code.ps

clean:
	rm *.o
	rm -f fgcomd lighthouse listener source-code.ps source-code.pdf

fgcomd: fgcomd.o config.o net.o user.o conference.o packet.o audio.o distance.o 
	$(CC) -o fgcomd fgcomd.o config.o net.o user.o conference.o packet.o audio.o distance.o $(LIBS)

lighthouse: lighthouse.o packet.o net.o
	$(CC) -o lighthouse lighthouse.o packet.o $(LIBS)

stress: stress.o packet.o net.o
	$(CC) -o stress stress.o packet.o $(LIBS)

listener: listener.o packet.o net.o
	$(CC) -o listener listener.o packet.o $(LIBS) -lopenal -lalut

fgcomd.o: fgcomd.c fgcomd.h config.h net.h user.h conference.h audio.h distance.h
	$(CC) $(CFLAGS) $(SVNDEF) -c fgcomd.c

config.o: config.c config.h
	$(CC) $(CFLAGS) -c config.c

net.o: net.c net.h packet.h
	$(CC) $(CFLAGS) -c net.c

user.o: user.c user.h packet.h
	$(CC) $(CFLAGS) -c user.c

conference.o: conference.c conference.h
	$(CC) $(CFLAGS) -c conference.c

packet.o: packet.c packet.h
	$(CC) $(CFLAGS) -c packet.c

distance.o: distance.c fgcomd.h distance.h
	$(CC) $(CFLAGS) -c distance.c

audio.o: audio.c audio.h
	$(CC) $(CFLAGS) -c audio.c

lighthouse.o: lighthouse.c fgcomd.h packet.c packet.h net.c net.h
	$(CC) $(CFLAGS) -c lighthouse.c

listener.o: listener.c fgcomd.h packet.c packet.h net.c net.h
	$(CC) $(CFLAGS) -c listener.c
