#define _GNU_SOURCE             /* MAP_ANONYMOUS, MAP_POPULATE */

#include "mcorelib.h"

#include <stdlib.h>
#include <sys/mman.h>

enum {
        // The number of blocks to request from the OS at a time
        REQUEST_BLOCKS = 32,
};

struct block
{
        struct block *next;
        char _pad[BLOCK_SIZE - sizeof(struct block*)];
};

static struct block *freelist;

static void *
refill(void)
{
        struct block *blocks =
                mmap(NULL, BLOCK_SIZE * REQUEST_BLOCKS, PROT_READ|PROT_WRITE,
                     MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE, -1, 0);
        if (blocks == MAP_FAILED)
                epanic("Failed to allocate blocks");

        // Construct a free list from the blocks, except the first,
        // which we're allocating.
        for (int i = 1; i < REQUEST_BLOCKS - 1; ++i)
                blocks[i].next = &blocks[i+1];
        struct block *head;
retry:
        head = freelist;
        blocks[REQUEST_BLOCKS-1].next = head;
        if (!__sync_bool_compare_and_swap(&freelist, head, &blocks[1]))
                goto retry;

        // Return the first block, which the caller wants to allocate
        return &blocks[0];
}

void *
Block_Alloc(void)
{
        struct block *block;
retry:
        block = freelist;
        if (!block)
                return refill();
        struct block *next = block->next;
        if (!__sync_bool_compare_and_swap(&freelist, block, next))
                goto retry;
        return block;
}

void
Block_Free(void *ptr)
{
        struct block *block = ptr;
        struct block *head;
retry:
        head = freelist;
        block->next = head;
        if (!__sync_bool_compare_and_swap(&freelist, head, block))
                goto retry;
}
