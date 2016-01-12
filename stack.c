/*

  Copyright (c) 2016 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#include "debug.h"
#include "slist.h"
#include "stack.h"
#include "utils.h"

/* Get memory page size. The query is done once only. The value is cached. */
static size_t dill_page_size(void) {
    static long pgsz = 0;
    if(dill_fast(pgsz))
        return (size_t)pgsz;
    pgsz = sysconf(_SC_PAGE_SIZE);
    dill_assert(pgsz > 0);
    return (size_t)pgsz;
}

/* Stack size, as specified by the user. */
static size_t dill_stack_size = 256 * 1024 - 256;
/* Actual stack size. */
static size_t dill_sanitised_stack_size = 0;

static size_t dill_get_stack_size(void) {
#if defined HAVE_POSIX_MEMALIGN && HAVE_MPROTECT
    /* If sanitisation was already done, return the precomputed size. */
    if(dill_fast(dill_sanitised_stack_size))
        return dill_sanitised_stack_size;
    dill_assert(dill_stack_size > dill_page_size());
    /* Amount of memory allocated must be multiply of the page size otherwise
       the behaviour of posix_memalign() is undefined. */
    size_t sz = (dill_stack_size + dill_page_size() - 1) &
        ~(dill_page_size() - 1);
    /* Allocate one additional guard page. */
    dill_sanitised_stack_size = sz + dill_page_size();
    return dill_sanitised_stack_size;
#else
    return dill_stack_size;
#endif
}

/* Maximum number of unused cached stacks. Keep in mind that we can't
   deallocate the stack you are running on. Thus we need at least one cached
   stack. */
static int dill_max_cached_stacks = 64;

/* A stack of unused coroutine stacks. This allows for extra-fast allocation
   of a new stack. The FIFO nature of this structure minimises cache misses.
   When the stack is cached its dill_slist_item is placed on its top rather
   then on the bottom. That way we minimise page misses. */
static int dill_num_cached_stacks = 0;
static struct dill_slist dill_cached_stacks = {0};

static void *dill_allocstackmem(void) {
    void *ptr;
#if defined HAVE_POSIX_MEMALIGN && HAVE_MPROTECT
    /* Allocate the stack so that it's memory-page-aligned. */
    int rc = posix_memalign(&ptr, dill_page_size(), dill_get_stack_size());
    if(dill_slow(rc != 0)) {
        errno = rc;
        return NULL;
    }
    /* The bottom page is used as a stack guard. This way stack overflow will
       cause segfault rather than randomly overwrite the heap. */
    rc = mprotect(ptr, dill_page_size(), PROT_NONE);
    if(dill_slow(rc != 0)) {
        int err = errno;
        free(ptr);
        errno = err;
        return NULL;
    }
#else
    ptr = malloc(dill_get_stack_size());
    if(dill_slow(!ptr)) {
        errno = ENOMEM;
        return NULL;
    }
#endif
    return (void*)(((char*)ptr) + dill_get_stack_size());
}

void *dill_allocstack(void) {
    if(!dill_slist_empty(&dill_cached_stacks)) {
        --dill_num_cached_stacks;
        return (void*)(dill_slist_pop(&dill_cached_stacks) + 1);
    }
    void *ptr = dill_allocstackmem();
    if(!ptr)
        dill_panic("not enough memory to allocate coroutine stack");
    return ptr;
}

void dill_freestack(void *stack) {
    /* Put the stack to the list of cached stacks. */
    struct dill_slist_item *item = ((struct dill_slist_item*)stack) - 1;
    dill_slist_push_back(&dill_cached_stacks, item);
    if(dill_num_cached_stacks < dill_max_cached_stacks) {
        ++dill_num_cached_stacks;
        return;
    }
    /* We can't deallocate the stack we are running on at the moment.
       Standard C free() is not required to work when it deallocates its
       own stack from underneath itself. Instead, we'll deallocate one of
       the unused cached stacks. */
    item = dill_slist_pop(&dill_cached_stacks);
    void *ptr = ((char*)(item + 1)) - dill_get_stack_size();
#if HAVE_POSIX_MEMALIGN && HAVE_MPROTECT
    int rc = mprotect(ptr, dill_page_size(), PROT_READ|PROT_WRITE);
    dill_assert(rc == 0);
#endif
    free(ptr);
}

