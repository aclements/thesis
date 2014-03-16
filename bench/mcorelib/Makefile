CFLAGS = -g -Wall -Werror -fno-omit-frame-pointer -O2
override CFLAGS += -std=c99
OBJS = msg.o cpu.o time.o block.o slab.o args.o rand.o rcu.o

mcorelib.a: $(OBJS)
	ar rcs $@ $^

$(OBJS): mcorelib.h args.h

rcutorture: rcutorture.o mcorelib.a
rcutorture: LDFLAGS+=-lpthread
rcutorture.o: mcorelib.h

clean:
	rm -f $(OBJS) mcorelib.a
