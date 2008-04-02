-include .config

MAKEFLAGS += --no-print-directory

CC ?= "gcc"
CFLAGS += -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration
CFLAGS += -I/lib/modules/`uname -r`/build/include
CFLAGS += -O2 -g
LDFLAGS += -lnl

OBJS = iw.o interface.o info.o station.o util.o mpath.o
ALL = iw
ifneq ($(shell which cgcc),"")
DEF := check
else
DEF := all
endif

default: $(DEF)

all: verify_config $(ALL)

iw:	$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o iw $(LDFLAGS)

check:
	@$(MAKE) all CC="REAL_CC=$(CC) CHECK=\"sparse -Wall\" cgcc"

clean:
	@rm -f iw *.o *~

verify_config:
	@if [ ! -r .config ]; then \
		echo 'Building iw requires a configuration file'; \
		echo '(.config). cp defconfig .config and edit.'; \
		exit 1; \
	fi
