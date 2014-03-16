#define _GNU_SOURCE             /* random_r */

#include "mcorelib.h"

#include <stdlib.h>

static __thread struct random_data rdata;
static __thread char random_state[256];
static __thread bool seeded;
static int seed;

int
Rand(void)
{
        int32_t res;

        if (!seeded) {
                int myseed = __sync_fetch_and_add(&seed, 1);
                if (initstate_r(myseed, random_state, sizeof(random_state), &rdata) < 0)
                        epanic("Failed to seed random generator");
                seeded = true;
        }

        random_r(&rdata, &res);
        return res;
}
