/*

  Copyright (c) 2015 Martin Sustrik

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
static size_t ts_page_size(void) {
    static long pgsz = 0;
    if(ts_fast(pgsz))
        return (size_t)pgsz;
    pgsz = sysconf(_SC_PAGE_SIZE);
    ts_assert(pgsz > 0);
    return (size_t)pgsz;
}

/* Stack size, as specified by the user. */
static size_t ts_stack_size = 256 * 1024 - 256;
/* Actual stack size. */
static size_t ts_sanitised_stack_size = 0;

static size_t ts_get_stack_size(void) {
#if defined HAVE_POSIX_MEMALIGN && HAVE_MPROTECT
    /* If sanitisation was already done, return the precomputed size. */
    if(ts_fast(ts_sanitised_stack_size))
        return ts_sanitised_stack_size;
    ts_assert(ts_stack_size > ts_page_size());
    /* Amount of memory allocated must be multiply of the page size otherwise
       the behaviour of posix_memalign() is undefined. */
    size_t sz = (ts_stack_size + ts_page_size() - 1) &
        ~(ts_page_size() - 1);
    /* Allocate one additional guard page. */
    ts_sanitised_stack_size = sz + ts_page_size();
    return ts_sanitised_stack_size;
#else
    return ts_stack_size;
#endif
}

/* Maximum number of unused cached stacks. Keep in mind that we can't
   deallocate the stack you are running on. Thus we need at least one cached
   stack. */
static int ts_max_cached_stacks = 64;

/* A stack of unused coroutine stacks. This allows for extra-fast allocation
   of a new stack. The FIFO nature of this structure minimises cache misses.
   When the stack is cached its ts_slist_item is placed on its top rather
   then on the bottom. That way we minimise page misses. */
static int ts_num_cached_stacks = 0;
static struct ts_slist ts_cached_stacks = {0};

static void *ts_allocstackmem(void) {
    void *ptr;
#if defined HAVE_POSIX_MEMALIGN && HAVE_MPROTECT
    /* Allocate the stack so that it's memory-page-aligned. */
    int rc = posix_memalign(&ptr, ts_page_size(), ts_get_stack_size());
    if(ts_slow(rc != 0)) {
        errno = rc;
        return NULL;
    }
    /* The bottom page is used as a stack guard. This way stack overflow will
       cause segfault rather than randomly overwrite the heap. */
    rc = mprotect(ptr, ts_page_size(), PROT_NONE);
    if(ts_slow(rc != 0)) {
        int err = errno;
        free(ptr);
        errno = err;
        return NULL;
    }
#else
    ptr = malloc(ts_get_stack_size());
    if(ts_slow(!ptr)) {
        errno = ENOMEM;
        return NULL;
    }
#endif
    return (void*)(((char*)ptr) + ts_get_stack_size());
}


void ts_preparestacks(int count, size_t stack_size) {
    /* Purge the cached stacks. */
    while(1) {
        struct ts_slist_item *item = ts_slist_pop(&ts_cached_stacks);
        if(!item)
            break;
        free(((char*)(item + 1)) - ts_get_stack_size());
    }
    /* Now that there are no stacks allocated, we can adjust the stack size. */
    size_t old_stack_size = ts_stack_size;
    size_t old_sanitised_stack_size = ts_sanitised_stack_size;
    ts_stack_size = stack_size;
    ts_sanitised_stack_size = 0;
    /* Allocate the new stacks. */
    int i;
    for(i = 0; i != count; ++i) {
        void *ptr = ts_allocstackmem();
        if(!ptr) goto error;
        struct ts_slist_item *item = ((struct ts_slist_item*)ptr) - 1;
        ts_slist_push_back(&ts_cached_stacks, item);
    }
    ts_num_cached_stacks = count;
    /* Make sure that the stacks won't get deallocated even if they aren't used
       at the moment. */
    ts_max_cached_stacks = count;
    errno = 0;
    return;
error:
    /* If we can't allocate all the stacks, allocate none, restore state and
       return error. */
    while(1) {
        struct ts_slist_item *item = ts_slist_pop(&ts_cached_stacks);
        if(!item)
            break;
        free(((char*)(item + 1)) - ts_get_stack_size());
    }
    ts_num_cached_stacks = 0;
    ts_stack_size = old_stack_size;
    ts_sanitised_stack_size = old_sanitised_stack_size;
    errno = ENOMEM;
}

void *ts_allocstack(void) {
    if(!ts_slist_empty(&ts_cached_stacks)) {
        --ts_num_cached_stacks;
        return (void*)(ts_slist_pop(&ts_cached_stacks) + 1);
    }
    void *ptr = ts_allocstackmem();
    if(!ptr)
        ts_panic("not enough memory to allocate coroutine stack");
    return ptr;
}

void ts_freestack(void *stack) {
    /* Put the stack to the list of cached stacks. */
    struct ts_slist_item *item = ((struct ts_slist_item*)stack) - 1;
    ts_slist_push_back(&ts_cached_stacks, item);
    if(ts_num_cached_stacks < ts_max_cached_stacks) {
        ++ts_num_cached_stacks;
        return;
    }
    /* We can't deallocate the stack we are running on at the moment.
       Standard C free() is not required to work when it deallocates its
       own stack from underneath itself. Instead, we'll deallocate one of
       the unused cached stacks. */
    item = ts_slist_pop(&ts_cached_stacks);
    void *ptr = ((char*)(item + 1)) - ts_get_stack_size();
#if HAVE_POSIX_MEMALIGN && HAVE_MPROTECT
    int rc = mprotect(ptr, ts_page_size(), PROT_READ|PROT_WRITE);
    ts_assert(rc == 0);
#endif
    free(ptr);
}

