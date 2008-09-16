-include .config

MAKEFLAGS += --no-print-directory

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

all: verify_config $(ALL)

%.o: %.c iw.h
	@$(NQ) ' CC  ' $@
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

iw:	$(OBJS)
	@$(NQ) ' CC  ' iw
	$(Q)$(CC) $(LDFLAGS) $(OBJS) -o iw

check:
	$(Q)$(MAKE) all CC="REAL_CC=$(CC) CHECK=\"sparse -Wall\" cgcc"

clean:
	$(Q)rm -f iw *.o *~

verify_config:
	$(Q)if [ ! -r .config ]; then \
		echo 'Building iw requires a configuration file'; \
		echo '(.config). cp defconfig .config and edit.'; \
		exit 1; \
	fi
