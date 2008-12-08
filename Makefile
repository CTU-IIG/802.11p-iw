-include .config

MAKEFLAGS += --no-print-directory

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

MKDIR ?= mkdir -p
INSTALL ?= install
CC ?= "gcc"

CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration `pkg-config --cflags libnl-1`
CFLAGS += -O2 -g
LDFLAGS += `pkg-config --libs libnl-1`
NLVERSION = 1.0

OBJS = iw.o info.o phy.o interface.o station.o util.o mpath.o reg.o mesh.o genl.o scan.o
ALL = iw

ifeq ($(V),1)
Q=
NQ=true
else
Q=@
NQ=echo
endif

all: version_check $(ALL)

version_check:
	@if ! pkg-config --atleast-version=$(NLVERSION) libnl-1; then echo "You need at least libnl version $(NLVERSION)"; exit 1; fi


version.h: version.sh
	@$(NQ) ' GEN  version.h'
	$(Q)./version.sh

%.o: %.c iw.h version.h
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
	$(Q)$(MKDIR) $(DESTDIR)$(BINDIR)
	$(Q)$(INSTALL) -m 755 -t $(DESTDIR)$(BINDIR) iw
	@$(NQ) ' INST iw.8'
	$(Q)$(MKDIR) $(DESTDIR)$(MANDIR)/man8/
	$(Q)$(INSTALL) -m 644 -t $(DESTDIR)$(MANDIR)/man8/ iw.8.gz

clean:
	$(Q)rm -f iw *.o *~ *.gz version.h *-stamp
