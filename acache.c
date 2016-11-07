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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "libdill.h"
#include "utils.h"
#include "osalloc.h"

dill_unique_id(acache_type);

struct acache_alloc {
    struct hvfs hvfs;
    struct alloc_vfs avfs;
    int flags;
    size_t sz, pgsz;
    void **cache;
    int cachesz, cachehead, cachetail, cachecnt;
};

#define DEFAULT_SIZE (256 * 1024)
#define DEFAULT_CACHESZ 64

static void *acache_hquery(struct hvfs *hvfs, const void *type);
static void acache_hclose(struct hvfs *hvfs);
static void *acache_guard(struct alloc_vfs *avfs);
static void *acache_memalign(struct alloc_vfs *avfs);
static void *acache_malloc(struct alloc_vfs *avfs);
static void *acache_calloc(struct alloc_vfs *avfs);
static int acache_free(struct alloc_vfs *avfs, void *stack);
static ssize_t acache_size(struct alloc_vfs *avfs);
static int acache_caps(struct alloc_vfs *avfs);

static void *acache_hquery(struct hvfs *hvfs, const void *type) {
    struct acache_alloc *obj = (struct acache_alloc *)hvfs;
    if(type == alloc_type) return &obj->avfs;
    if(type == acache_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int acache(int flags, size_t sz, size_t cachesz) {
#if !HAVE_MPROTECT
    if(flags & DILL_ALLOC_FLAGS_GUARD) {errno = ENOTSUP; return -1;}
#endif
    if(flags & DILL_ALLOC_FLAGS_GUARD) flags |= DILL_ALLOC_FLAGS_ALIGN;
    /* Check that the size underlying allocator can fit a pool block */
    if(dill_slow(!sz)) sz = DEFAULT_SIZE;
    struct acache_alloc *obj = malloc(sizeof(struct acache_alloc));
    if(dill_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = acache_hquery;
    obj->hvfs.close = acache_hclose;
    if(flags & DILL_ALLOC_FLAGS_ALIGN)
        obj->avfs.alloc = acache_memalign;
    else if(flags & DILL_ALLOC_FLAGS_ZERO)
        obj->avfs.alloc = acache_calloc;
    else
        obj->avfs.alloc = acache_malloc;
    obj->avfs.free = acache_free;
    obj->avfs.size = acache_size;
    obj->avfs.caps = acache_caps;
    obj->flags = flags;
    obj->sz = sz ? sz : DEFAULT_SIZE;
    obj->pgsz = dill_page_size();
    /* Pick element sizes that fit the minimum requirement of flags */
    if(flags & DILL_ALLOC_FLAGS_ALIGN)
        obj->sz = dill_align(obj->sz, obj->pgsz);
    /* As caches are fixed-size, allocate an extra page for guards early */
    if(flags & DILL_ALLOC_FLAGS_GUARD)
        obj->sz = dill_page_larger(obj->sz, obj->pgsz);
    obj->cachesz = cachesz ? cachesz : DEFAULT_CACHESZ;
    obj->cache = malloc(obj->cachesz * sizeof(void *));
    obj->cachehead = obj->cachetail = obj->cachecnt = 0;
    /* Create the handle. */
    int h = hcreate(&obj->hvfs);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(obj);
        errno = err;
        return -1;
    }
    return h;
}

static inline void *acache_pop(struct acache_alloc *obj) {
    if(!obj->cachecnt) return NULL;
    void *ptr = obj->cache[obj->cachehead];
    obj->cachehead = (obj->cachehead + 1) % obj->cachesz;
    obj->cachecnt--;
    if(obj->flags & DILL_ALLOC_FLAGS_ZERO) {
        int guard = (obj->flags & DILL_ALLOC_FLAGS_GUARD);
        memset(guard ? ptr + obj->pgsz : ptr, 0,
            guard ? obj->sz - obj->pgsz : obj->sz);
    }
    return ptr;
}

static inline int acache_push(struct acache_alloc *obj, void *p) {
    if(obj->cachecnt != obj->cachesz) {
        obj->cache[obj->cachetail] = p;
        obj->cachetail = (obj->cachetail + 1) % obj->cachesz;
        obj->cachecnt++;
        return 0;
    }
    return -1;
}

#define acache_a(method, avfs) ({\
    struct acache_alloc *obj =\
        dill_cont(avfs, struct acache_alloc, avfs);\
    void *stack = acache_pop(obj);\
    if(!stack) {\
        stack = dill_alloc(method, obj->flags & DILL_ALLOC_FLAGS_ZERO,\
            obj->pgsz, obj->sz);\
        if(dill_slow(!stack)) return NULL;\
        if(obj->flags & DILL_ALLOC_FLAGS_GUARD) {\
            if(dill_guard(stack, obj->pgsz) == -1) {\
                free(stack); return NULL;}\
        }\
    }\
    stack + obj->sz;\
})

static void *acache_memalign(struct alloc_vfs *avfs) {
    return acache_a(memalign, avfs);
}

static void *acache_malloc(struct alloc_vfs *avfs) {
    return acache_a(malloc, avfs);
}

static void *acache_calloc(struct alloc_vfs *avfs) {
    return acache_a(calloc, avfs);
}

static int acache_free(struct alloc_vfs *avfs, void *stack) {
    struct acache_alloc *obj =
        dill_cont(avfs, struct acache_alloc, avfs);
    void *ptr = stack - obj->sz;
    if(acache_push(obj, ptr) == -1) {
        if(obj->flags & DILL_ALLOC_FLAGS_GUARD) {
            int rc = dill_unguard(ptr, obj->pgsz);
            if(rc) {errno = rc; return -1;}
        }
        free(ptr);
    }
    return 0;
}

static ssize_t acache_size(struct alloc_vfs *avfs) {
    struct acache_alloc *obj =
        dill_cont(avfs, struct acache_alloc, avfs);
    return obj->sz;
}

static int acache_caps(struct alloc_vfs *avfs) {
    struct acache_alloc *obj =
        dill_cont(avfs, struct acache_alloc, avfs);
    int caps = 0;
    if(obj->flags & (DILL_ALLOC_FLAGS_ZERO)) caps |= DILL_ALLOC_CAPS_ZERO;
    if(obj->flags & (DILL_ALLOC_FLAGS_ALIGN)) caps |= DILL_ALLOC_CAPS_ALIGN;
    if(obj->flags & (DILL_ALLOC_FLAGS_GUARD)) caps |= DILL_ALLOC_CAPS_GUARD;
    return caps;
}

static void acache_hclose(struct hvfs *hvfs) {
    struct acache_alloc *obj = (struct acache_alloc *)hvfs;
    /* This won't free all memory allocated if memory was allocated not tracked by the cache */
    if(obj->cachecnt) {
        while(obj->cachehead != obj->cachetail) {
            free(obj->cache[obj->cachehead]);
            obj->cachehead = (obj->cachehead + 1) % obj->cachesz;
        }
    }
    free(obj->cache);
    free(obj);
}
