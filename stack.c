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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#include "stack.h"
#include "utils.h"
#include "ctx.h"

/* The stacks are cached. The advantage of this is twofold. First, caching is
   faster than malloc(). Second, it results in fewer calls to
   mprotect(). */

/* Stack size in bytes. */
static size_t dill_stack_size = 256 * 1024;
/* Maximum number of unused cached stacks. Must be at least 1. */
static int dill_max_cached_stacks = 64;

/* Returns the smallest value that's greater than val and is a multiple of unit. */
static size_t dill_align(size_t val, size_t unit) {
    return val % unit ? val + unit - val % unit : val;
}

int dill_stack_set_default_size(size_t sz) {
    if (sz < 8192 || (sz % 1024) != 0) {
        errno = EINVAL;
        return -1;
    }
    else {
        dill_stack_size = sz;
        errno = 0;
        return 0;
    }
}

int dill_stack_set_cache_max(int nb) {
    if (nb <= 0) {
        errno = EINVAL;
        return -1;
    }
    else {
        dill_max_cached_stacks = nb;
        errno = 0;
        return 0;
    }
}

/* Get memory page size. The query is done once. The value is cached. */
static size_t dill_page_size(void) {
    static long pgsz = 0;
    if(dill_fast(pgsz))
        return (size_t)pgsz;
    pgsz = sysconf(_SC_PAGE_SIZE);
    dill_assert(pgsz > 0);
    return (size_t)pgsz;
}

int dill_ctx_stack_init(struct dill_ctx_stack *ctx) {
    ctx->count = 0;
    dill_slist_init(&ctx->cache);
    return 0;
}

void dill_ctx_stack_term(struct dill_ctx_stack *ctx) {
    /* Deallocate leftover coroutines. */
    struct dill_slist *it;
    while((it = dill_slist_pop(&ctx->cache)) != &ctx->cache) {
#if (HAVE_POSIX_MEMALIGN && HAVE_MPROTECT) & !defined DILL_NOGUARD
        void *ptr = ((uint8_t*)(it + 1)) - dill_stack_size - dill_page_size();
        int rc = mprotect(ptr, dill_page_size(), PROT_READ|PROT_WRITE);
        dill_assert(rc == 0);
        free(ptr);
#else
        void *ptr = ((uint8_t*)(it + 1)) - dill_stack_size;
        free(ptr);
#endif
    }
}

void *dill_allocstack(size_t *stack_size) {
    struct dill_ctx_stack *ctx = &dill_getctx->stack;
    if(stack_size)
        *stack_size = dill_stack_size;
    /* If there's a cached stack, use it. */
    if(!dill_slist_empty(&ctx->cache)) {
        --ctx->count;
        return (void*)(dill_slist_pop(&ctx->cache) + 1);
    }
    /* Allocate a new stack. */
    uint8_t *top;
#if (HAVE_POSIX_MEMALIGN && HAVE_MPROTECT) & !defined DILL_NOGUARD
    /* Allocate the stack so that it's memory-page-aligned.
       Add one page as a stack overflow guard. */
    size_t sz = dill_align(dill_stack_size, dill_page_size()) +
        dill_page_size();
    uint8_t *ptr;
    int rc = posix_memalign((void**)&ptr, dill_page_size(), sz);
    if(dill_slow(rc != 0)) {
        errno = rc;
        return NULL;
    }
    /* The bottom page is used as a stack guard. This way a stack overflow will
       cause a segfault instead of randomly overwriting the heap. */
    rc = mprotect(ptr, dill_page_size(), PROT_NONE);
    if(dill_slow(rc != 0)) {
        int err = errno;
        free(ptr);
        errno = err;
        return NULL;
    }
    top = ptr + dill_page_size() + dill_stack_size;
#else
    /* Simple allocation without a guard page. */
    uint8_t *ptr = malloc(dill_stack_size);
    if(dill_slow(!ptr)) {
        errno = ENOMEM;
        return NULL;
    }
    top = ptr + dill_stack_size;
#endif
    return top;
}

void dill_freestack(void *stack) {
    struct dill_ctx_stack *ctx = &dill_getctx->stack;
    struct dill_slist *item = ((struct dill_slist*)stack) - 1;
    /* If the cache is full we will deallocate one stack from the cache.
       We can't deallocate the stack passed to this function directly because
       this very function can be still executing on that stack. */
    if(ctx->count >= dill_max_cached_stacks) {
        struct dill_slist *old = dill_slist_pop(&ctx->cache);
        --ctx->count;
#if (HAVE_POSIX_MEMALIGN && HAVE_MPROTECT) & !defined DILL_NOGUARD
        void *ptr = ((uint8_t*)(old + 1)) - dill_stack_size - dill_page_size();
        int rc = mprotect(ptr, dill_page_size(), PROT_READ|PROT_WRITE);
        dill_assert(rc == 0);
        free(ptr);
#else
        void *ptr = ((uint8_t*)(old + 1)) - dill_stack_size;
        free(ptr);
#endif
    }
    /* Put the stack into the cache. */
    dill_slist_push(&ctx->cache, item);
    ++ctx->count;
}

