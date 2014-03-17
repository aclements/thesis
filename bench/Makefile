CFLAGS += -std=gnu99 -g -O2 -Wall

all: write-sharing

write-sharing: write-sharing.c mcorelib/mcorelib.a
write-sharing: CFLAGS += -lpthread -lnuma -lm

mcorelib/mcorelib.a: FORCE
	$(MAKE) -C mcorelib

clean:
	rm -f write-sharing
	$(MAKE) -C mcorelib clean

.PHONY: FORCE
