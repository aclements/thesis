#define _GNU_SOURCE             /* nanosleep */

#include "mcorelib.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define NWRITERS 2
#define NREADERS 2
#define PIPELINE 10

struct ent
{
        struct RCU_Link rcu;
        int pipe;
};

struct ent *current[NWRITERS];

struct counters
{
        uint64_t readerPipe[PIPELINE];
        uint64_t freePipe[PIPELINE];
        uint64_t alloc, free;
} counters[NWRITERS+NREADERS];

__thread struct counters *myCounters;

uint64_t outstanding;
bool freeze;

void
release(struct RCU_Link *link)
{
        struct ent *ent = container_of(link, struct ent, rcu);
        while (ACCESS_ONCE(freeze));
        if (++ent->pipe < PIPELINE) {
                myCounters->freePipe[ent->pipe - 1]++;
                RCU_Call(&ent->rcu, release);
        } else {
                free(ent);
                myCounters->free++;
                __sync_fetch_and_sub(&outstanding, 1);
        }
}

void *
writer(void *opaque)
{
        int id = (int)(uintptr_t)opaque;
        struct ent *prev = NULL;
        myCounters = &counters[id];

        while (1) {
                // XXX malloc is slow.  Would be better if we could
                // move things faster.
                struct ent *ent = malloc(sizeof *ent);
                if (!ent)
                        epanic("malloc");
                myCounters->alloc++;
                __sync_fetch_and_add(&outstanding, 1);
                ent->pipe = 0;
                current[id] = ent;

                if (prev)
                        release(&prev->rcu);

                prev = ent;

                while (ACCESS_ONCE(outstanding) > 100000) {
                        struct timespec ts = {0, 1000};
                        nanosleep(&ts, NULL);
                        RCU_Flush();
                }

//                for (int i = 0; i < 10000; i++);
        }
}

void *
reader(void *opaque)
{
        int id = (int)(uintptr_t)opaque;
        int pos = 0, pipe;
        myCounters = &counters[id];

        while (1) {
                struct ent *ent;
                while (ACCESS_ONCE(freeze));
                RCU_ReadLock();
                // XXX Sleep
                ent = ACCESS_ONCE(current[pos]);
                if (!ent)
                        goto skip;
                // XXX Sleep
                pipe = ACCESS_ONCE(ent->pipe);
                if (pipe >= PIPELINE)
                        pipe = PIPELINE - 1;
                myCounters->readerPipe[pipe]++;
                // XXX Sleep
        skip:
                RCU_ReadUnlock();

                pos = (pos + 1) % NWRITERS;
        }
}

int
main(int argc, char **argv)
{
        pthread_t threads[NWRITERS+NREADERS];
        int i, j;

        for (i = 0; i < NWRITERS + NREADERS; ++i) {
                if (pthread_create(&threads[i], NULL,
                                   i < NWRITERS ? writer : reader,
                                   (void*)(uintptr_t)i) < 0)
                        epanic("pthread_create");
        }

        while (1) {
                struct counters total = {};

                freeze = true;

                for (i = 0; i < NWRITERS + NREADERS; ++i) {
                        for (j = 0; j < PIPELINE; ++j) {
                                total.readerPipe[j] += counters[i].readerPipe[j];
                                total.freePipe[j] += counters[i].freePipe[j];
                        }
                        total.alloc += counters[i].alloc;
                        total.free += counters[i].free;
                }

                printf("Reader pipe:");
                for (j = 0; j < PIPELINE; ++j)
                        printf(" %"PRIu64, total.readerPipe[j]);
                for (j = 2; j < PIPELINE; ++j)
                        if (total.readerPipe[j]) {
                                printf(" !!!");
                                break;
                        }
                printf("\n");

                printf("Free pipe:  ");
                for (j = 0; j < PIPELINE; ++j)
                        printf(" %"PRIu64, total.freePipe[j]);
                printf("\n");

                printf("alloc: %"PRIu64"  free: %"PRIu64"  diff: %"PRIu64"\n",
                       total.alloc, total.free, total.alloc - total.free);

                RCU_DumpState();

                freeze = false;

                sleep(5);
                printf("\n");
        }
}
