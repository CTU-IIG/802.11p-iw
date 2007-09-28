# adjust this
LIBNL = /home/johannes/Projects/libnl/

CFLAGS += -Wall -lnl -I$(LIBNL)/include/ -L$(LIBNL)/lib/

iw:	iw.c iw.h

clean:
	rm -f iw *~
