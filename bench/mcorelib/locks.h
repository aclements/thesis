#ifndef _MCORELIB_LOCKS_H
#define _MCORELIB_LOCKS_H

// Ticket locks, copied from Linux

typedef struct TicketLock
{
        // The low 8 bits store the currently serving ticket number.
        // The next 8 bits store the next available ticket number.
        // This can support up to 256 threads vying for the same lock
        // at the same time.
        unsigned int slock;
} TicketLock_t;

#if __i386
# define REG_PTR_MODE "k"
#elif __x86_64
# define REG_PTR_MODE "q"
#else
# error Unknown architecture
#endif

static inline __attribute__((__always_inline__)) void
TicketLock_Lock(TicketLock_t *lock)
{
	short inc = 0x0100;

	__asm__ __volatile__(
		"lock; xaddw %w0, %1\n"
		"1:\t"
		"cmpb %h0, %b0\n\t"
		"je 2f\n\t"
		"rep ; nop\n\t"
		"movb %1, %b0\n\t"
		/* don't need lfence here, because loads are in-order */
		"jmp 1b\n"
		"2:"
		: "+Q" (inc), "+m" (lock->slock)
		:
		: "memory", "cc");
}

static inline __attribute__((__always_inline__)) int
TicketLock_TryLock(TicketLock_t *lock)
{
	int tmp, new;

	__asm__ __volatile__(
                "movzwl %2, %0\n\t"
                "cmpb %h0,%b0\n\t"
                "leal 0x100(%" REG_PTR_MODE "0), %1\n\t"
                "jne 1f\n\t"
                "lock; cmpxchgw %w1,%2\n\t"
                "1:"
                "sete %b1\n\t"
                "movzbl %b1,%0\n\t"
                : "=&a" (tmp), "=&q" (new), "+m" (lock->slock)
                :
                : "memory", "cc");

	return tmp;
}

static inline __attribute__((__always_inline__)) void
TicketLock_Unlock(TicketLock_t *lock)
{
	__asm__ __volatile__(
                "incb %0"
                : "+m" (lock->slock)
                :
                : "memory", "cc");
}

#undef REG_PTR_MODE

#endif // _MCORELIB_LOCKS_H
