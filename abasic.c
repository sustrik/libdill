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

dill_unique_id(abasic_type);

struct abasic_alloc {
    struct hvfs hvfs;
    struct alloc_vfs avfs;
    int flags;
    size_t sz;
    size_t pgsz;
};

#define DEFAULT_SIZE (256 * 1024)

static void *abasic_hquery(struct hvfs *hvfs, const void *type);
static void abasic_hclose(struct hvfs *hvfs);
static void *abasic_guard(struct alloc_vfs *avfs);
static void *abasic_memalign(struct alloc_vfs *avfs);
static void *abasic_malloc(struct alloc_vfs *avfs);
static void *abasic_calloc(struct alloc_vfs *avfs);
static int abasic_unguard_free(struct alloc_vfs *avfs, void *stack);
static int abasic_free(struct alloc_vfs *avfs, void *stack);
static ssize_t abasic_size(struct alloc_vfs *avfs);
static int abasic_caps(struct alloc_vfs *avfs);

static void *abasic_hquery(struct hvfs *hvfs, const void *type) {
    struct abasic_alloc *obj = (struct abasic_alloc *)hvfs;
    if(type == alloc_type) return &obj->avfs;
    if(type == abasic_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int abasic(int flags, size_t sz) {
#if !HAVE_MPROTECT
    if(flags & DILL_ALLOC_FLAGS_GUARD) {errno = ENOTSUP; return -1;}
#endif
    if(flags & DILL_ALLOC_FLAGS_GUARD) flags |= DILL_ALLOC_FLAGS_ALIGN;
    struct abasic_alloc *obj = malloc(sizeof(struct abasic_alloc));
    if(dill_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = abasic_hquery;
    obj->hvfs.close = abasic_hclose;
    if(flags & DILL_ALLOC_FLAGS_GUARD)
        obj->avfs.alloc = abasic_guard;
    else if(flags & DILL_ALLOC_FLAGS_ALIGN)
        obj->avfs.alloc = abasic_memalign;
    else if(flags & DILL_ALLOC_FLAGS_ZERO)
        obj->avfs.alloc = abasic_calloc;
    else
        obj->avfs.alloc = abasic_malloc;
    obj->avfs.free = (flags & DILL_ALLOC_FLAGS_GUARD) ?
        abasic_unguard_free : abasic_free;
    obj->avfs.free = abasic_free;
    obj->avfs.size = abasic_size;
    obj->avfs.caps = abasic_caps;
    obj->flags = flags;
    obj->pgsz = dill_page_size();
    obj->sz = sz;
    if(flags & DILL_ALLOC_FLAGS_ALIGN)
        obj->sz = dill_align(obj->sz, obj->pgsz);
    /* With the stack guard, this function will always add more memory
       than requested.  Always use this value to calculate the top of the
       stack, if this is used directly instead of pool or a cache wrappers. */
    if(flags & DILL_ALLOC_FLAGS_GUARD)
        obj->sz = dill_page_larger(obj->sz, obj->pgsz);
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

static void *abasic_guard(struct alloc_vfs *avfs) {
    struct abasic_alloc *obj = dill_cont(avfs, struct abasic_alloc, avfs);
    void *stack = dill_alloc(memalign,
        obj->flags & DILL_ALLOC_FLAGS_ZERO, obj->pgsz, obj->sz);
    if(dill_guard(stack, obj->pgsz) == -1) {free(stack); return NULL;}
    if(dill_slow(!stack)) return NULL;
    return stack + obj->sz;
}

#define abasic_a(method, avfs) ({\
    struct abasic_alloc *obj = dill_cont(avfs, struct abasic_alloc, avfs);\
    void *stack = dill_alloc(method,\
        obj->flags & DILL_ALLOC_FLAGS_ZERO, obj->pgsz, obj->sz);\
    if(dill_slow(!stack)) return NULL;\
    stack + obj->sz;\
})

static void *abasic_memalign(struct alloc_vfs *avfs) {
    return abasic_a(memalign, avfs);
}

static void *abasic_malloc(struct alloc_vfs *avfs) {
    return abasic_a(malloc, avfs);
}

static void *abasic_calloc(struct alloc_vfs *avfs) {
    return abasic_a(calloc, avfs);
}

static int abasic_unguard_free(struct alloc_vfs *avfs, void *stack) {
    struct abasic_alloc *obj =
        dill_cont(avfs, struct abasic_alloc, avfs);
    int rc = dill_unguard(stack - obj->sz, obj->pgsz);
    if(rc) {errno = rc; return -1;}
    free(stack - obj->sz);
    return 0;
}

static int abasic_free(struct alloc_vfs *avfs, void *stack) {
    struct abasic_alloc *obj =
        dill_cont(avfs, struct abasic_alloc, avfs);
    free(stack - obj->sz);
    return 0;
}

static ssize_t abasic_size(struct alloc_vfs *avfs) {
    struct abasic_alloc *obj =
        dill_cont(avfs, struct abasic_alloc, avfs);
    return obj->sz;
}

static int abasic_caps(struct alloc_vfs *avfs) {
    struct abasic_alloc *obj =
        dill_cont(avfs, struct abasic_alloc, avfs);
    int caps = 0;
    if(obj->flags & (DILL_ALLOC_FLAGS_ZERO)) caps |= DILL_ALLOC_CAPS_ZERO;
    if(obj->flags & (DILL_ALLOC_FLAGS_ALIGN)) caps |= DILL_ALLOC_CAPS_ALIGN;
    if(obj->flags & (DILL_ALLOC_FLAGS_GUARD)) caps |= DILL_ALLOC_CAPS_GUARD;
    return caps;
}

static void abasic_hclose(struct hvfs *hvfs) {
    struct abasic_alloc *obj = (struct abasic_alloc *)hvfs;
    /* This won't free memory that is allocated there is no book-keeping */
    free(obj);
}
