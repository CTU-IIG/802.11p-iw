-include .config

MAKEFLAGS += --no-print-directory

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man

MKDIR ?= mkdir -p
INSTALL ?= install
CC ?= "gcc"

CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration

OBJS = iw.o genl.o info.o phy.o interface.o station.o util.o mesh.o mpath.o reg.o
ALL = iw

NL1FOUND := $(shell pkg-config --atleast-version=1 libnl-1 && echo Y)
NL2FOUND := $(shell pkg-config --atleast-version=2 libnl-2.0 && echo Y)

ifeq ($(NL1FOUND),Y)
NLLIBNAME = libnl-1
endif

ifeq ($(NL2FOUND),Y)
CFLAGS += -DCONFIG_LIBNL20
LIBS += -lnl-genl
NLLIBNAME = libnl-2.0
endif

ifeq ($(NLLIBNAME),)
$(error Cannot find development files for any supported version of libnl)
endif

LIBS += $(shell pkg-config --libs $(NLLIBNAME))
CFLAGS += $(shell pkg-config --cflags $(NLLIBNAME))

ifeq ($(V),1)
Q=
NQ=true
else
Q=@
NQ=echo
endif

all: version_check $(ALL)

version_check:
ifeq ($(NL2FOUND),Y)
else
ifeq ($(NL1FOUND),Y)
else
	$(error No libnl found)
endif
endif

version.h: version.sh
	@$(NQ) ' GEN  version.h'
	$(Q)./version.sh

%.o: %.c iw.h version.h nl80211.h
	@$(NQ) ' CC  ' $@
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

iw:	$(OBJS)
	@$(NQ) ' CC  ' iw
	$(Q)$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o iw

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
