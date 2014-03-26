CFLAGS += -std=gnu99 -g -O2 -Wall

all: sharing

sharing: sharing.c mcorelib/mcorelib.a
sharing: CFLAGS += -lpthread -lnuma -lm

mcorelib/mcorelib.a: FORCE
	$(MAKE) -C mcorelib

clean:
	rm -f sharing
	$(MAKE) -C mcorelib clean

.PHONY: FORCE
