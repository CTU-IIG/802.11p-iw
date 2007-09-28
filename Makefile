# adjust this
LIBNL = /home/johannes/Projects/libnl/

CFLAGS += -Wall -I$(LIBNL)/include/ -g
LDFLAGS += -lnl -L$(LIBNL)/lib/

iw:	iw.o interface.o

clean:
	rm -f iw *.o *~
