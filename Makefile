-include .config

MAKEFLAGS += --no-print-directory

INSTALL ?= install
PREFIX ?= /usr
CC ?= "gcc"
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration
CFLAGS += -I/lib/modules/`uname -r`/build/include
CFLAGS += -O2 -g
LDFLAGS += -lnl

OBJS = iw.o info.o phy.o interface.o station.o util.o mpath.o reg.o
ALL = iw

ifeq ($(V),1)
Q=
NQ=true
else
Q=@
NQ=echo
endif

all: $(ALL)

%.o: %.c iw.h
	@$(NQ) ' CC  ' $@
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

iw:	$(OBJS)
	@$(NQ) ' CC  ' iw
	$(Q)$(CC) $(LDFLAGS) $(OBJS) -o iw

check:
	$(Q)$(MAKE) all CC="REAL_CC=$(CC) CHECK=\"sparse -Wall\" cgcc"

%.gz: %
	@$(NQ) ' GZIP' $<
	$(Q)gzip < $< > $@

install: iw iw.8.gz
	@$(NQ) ' INST iw'
	$(Q)$(INSTALL) -o root -g root -t $(PREFIX)/bin iw
	@$(NQ) ' INST iw.8'
	$(Q)$(INSTALL) -o root -g root -t $(PREFIX)/share/man/man8/ iw.8.gz

clean:
	$(Q)rm -f iw *.o *~ *.gz
