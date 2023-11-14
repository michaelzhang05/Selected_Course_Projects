CC=gcc

CFLAGS = -g -c -Wall -pedantic

all: rcv ncp t_rcv t_ncp


rcv: rcv.o
	    $(CC) -o rcv rcv.o  

ncp: ncp.o
	    $(CC) -o ncp ncp.o

t_rcv: t_rcv.o
	    $(CC) -o t_rcv t_rcv.o

t_ncp: t_ncp.o
	    $(CC) -o t_ncp t_ncp.o
		


clean:
	rm *.o
	rm rcv
	rm ncp
	rm t_rcv
	rm t_ncp
	

%.o:    %.c
	$(CC) $(CFLAGS) $*.c


