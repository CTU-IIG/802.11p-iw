-include .config

CC ?= "gcc"
CFLAGS += -Wall -I/lib/modules/`uname -r`/build/include -g
LDFLAGS += -lnl

OBJS = iw.o interface.o
ALL = iw

all: verify_config $(ALL)

iw:	$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o iw $(LDFLAGS)

clean:
	rm -f iw *.o *~

verify_config:
	@if [ ! -r .config ]; then \
		echo 'Building iw requires a configuration file'; \
		echo '(.config). See README for more instructions. You can'; \
		echo 'run "cp defconfig .config" to create an example'; \
		echo 'configuration.'; \
		exit 1; \
	fi
