CFLAGS += -std=gnu99 -g -O2 -Wall

all: sharing memscan

sharing: sharing.c mcorelib/mcorelib.a
sharing: CFLAGS += -lpthread -lnuma -lm

memscan: memscan.c mcorelib/mcorelib.a
memscan: CFLAGS += -lpthread -lnuma -lm

mcorelib/mcorelib.a: FORCE
	$(MAKE) -C mcorelib

clean:
	rm -f sharing memscan
	$(MAKE) -C mcorelib clean

.PHONY: FORCE
