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

dill_unique_id(amalloc_type);

struct amalloc_alloc {
    struct hvfs hvfs;
    struct alloc_vfs avfs;
    int flags;
    size_t sz;
};

#define DEFAULT_SIZE (256 * 1024)

static void *amalloc_hquery(struct hvfs *hvfs, const void *type);
static void amalloc_hclose(struct hvfs *hvfs);
static void *amalloc_alloc(struct alloc_vfs *avfs, size_t *sz);
static void *amalloc_calloc(struct alloc_vfs *avfs, size_t *sz);
static int amalloc_free(struct alloc_vfs *avfs, void *p);
static ssize_t amalloc_size(struct alloc_vfs *avfs);
static int amalloc_caps(struct alloc_vfs *avfs);

static void *amalloc_hquery(struct hvfs *hvfs, const void *type) {
    struct amalloc_alloc *obj = (struct amalloc_alloc *)hvfs;
    if(type == alloc_type) return &obj->avfs;
    if(type == amalloc_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int amalloc(int flags, size_t sz) {
    if(flags & (DILL_ALLOC_FLAGS_GUARD | DILL_ALLOC_FLAGS_HUGE)) {
        errno = ENOTSUP; return -1;
    }
    struct amalloc_alloc *obj = malloc(sizeof(struct amalloc_alloc));
    if(dill_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = amalloc_hquery;
    obj->hvfs.close = amalloc_hclose;
    obj->avfs.alloc = (flags & DILL_ALLOC_FLAGS_ZERO)
        ? amalloc_calloc : amalloc_alloc;
    obj->avfs.free = amalloc_free;
    obj->avfs.size = amalloc_size;
    obj->avfs.caps = amalloc_caps;
    obj->flags = flags;
    obj->sz = sz ? sz : DEFAULT_SIZE;
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

static void *amalloc_alloc(struct alloc_vfs *avfs, size_t *sz) {
    struct amalloc_alloc *obj =
        dill_cont(avfs, struct amalloc_alloc, avfs);
    /* Try use *sz first (if valid), if not use default & return through *sz */
    size_t size = sz ? (*sz ? *sz : (*sz = obj->sz)) : obj->sz;
    void *m = malloc(size);
    if(dill_slow(!m)) return NULL;
    return m;
}

static void *amalloc_calloc(struct alloc_vfs *avfs, size_t *sz) {
    struct amalloc_alloc *obj =
        dill_cont(avfs, struct amalloc_alloc, avfs);
    /* Try use *sz first (if valid), if not use default & return through *sz */
    size_t size = sz ? (*sz ? *sz : (*sz = obj->sz)) : obj->sz;
    void *m = calloc(1, size);
    if(dill_slow(!m)) return NULL;
    return m;
}

static int amalloc_free(struct alloc_vfs *avfs, void *p) {
    free(p);
    return 0;
}

static ssize_t amalloc_size(struct alloc_vfs *avfs) {
    struct amalloc_alloc *obj =
        dill_cont(avfs, struct amalloc_alloc, avfs);
    return obj->sz;
}

static int amalloc_caps(struct alloc_vfs *avfs) {
    struct amalloc_alloc *obj =
        dill_cont(avfs, struct amalloc_alloc, avfs);
    int caps = DILL_ALLOC_CAPS_RESIZE;
    if(obj->flags & (DILL_ALLOC_FLAGS_ZERO)) caps |= DILL_ALLOC_CAPS_ZERO;
    return caps;
}

static void amalloc_hclose(struct hvfs *hvfs) {
    struct amalloc_alloc *obj = (struct amalloc_alloc *)hvfs;
    /* This won't free memory that is allocated because it does not
       expose DILL_CAPS_BOOKKEEP capability */
    free(obj);
}
