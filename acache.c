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

dill_unique_id(acache_type);

struct acache_alloc {
    struct hvfs hvfs;
    struct alloc_vfs avfs;
    struct alloc_vfs *iavfs;
    int flags;
    size_t sz;
    void **cache;
    int cachesz, cachehead, cachetail, cachecnt;
};

#define DEFAULT_SIZE (256 * 1024)
#define DEFAULT_CACHESZ 64

static void *acache_hquery(struct hvfs *hvfs, const void *type);
static void acache_hclose(struct hvfs *hvfs);
static void *acache_alloc(struct alloc_vfs *avfs, size_t *sz);
static int acache_free(struct alloc_vfs *avfs, void *p);
static ssize_t acache_size(struct alloc_vfs *avfs);
static int acache_caps(struct alloc_vfs *avfs);

static void *acache_hquery(struct hvfs *hvfs, const void *type) {
    struct acache_alloc *obj = (struct acache_alloc *)hvfs;
    if(type == alloc_type) return &obj->avfs;
    if(type == acache_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int acache(int a, int flags, size_t sz, size_t cachesz) {
    if(flags & (DILL_ALLOC_FLAGS_GUARD | DILL_ALLOC_FLAGS_HUGE)) {
        errno = ENOTSUP; return -1;
    }
    /* Get underlying allocator type */
    struct alloc_vfs *iavfs = hquery(a, alloc_type);
    /* Check whether underlying type is an allocator */
    if(dill_slow(!iavfs)) return -1;
    /* Check that the size underlying allocator can fit a pool block */
    if(dill_slow(!sz)) sz = DEFAULT_SIZE;
    if(dill_slow(!(iavfs->caps(iavfs) & DILL_ALLOC_CAPS_RESIZE)))
        if(sz > iavfs->size(iavfs)) {errno = ENOTSUP; return -1;}
    struct acache_alloc *obj = malloc(sizeof(struct acache_alloc));
    if(dill_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = acache_hquery;
    obj->hvfs.close = acache_hclose;
    obj->avfs.alloc = acache_alloc;
    obj->avfs.free = acache_free;
    obj->avfs.size = acache_size;
    obj->avfs.caps = acache_caps;
    obj->iavfs = iavfs;
    obj->flags = flags;
    obj->sz = sz ? sz : DEFAULT_SIZE;
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

static void *acache_alloc(struct alloc_vfs *avfs, size_t *sz) {
    struct acache_alloc *obj =
        dill_cont(avfs, struct acache_alloc, avfs);
    void *m;
    /* Always use the size given in the cache */
    if(*sz > obj->sz) {errno = ENOTSUP; return NULL;}
    /* Try use *sz first (if valid), if not use default & return through *sz */
    size_t size = sz ? (*sz ? *sz : (*sz = obj->sz)) : obj->sz;
    if(obj->cachecnt) {
        m = obj->cache[obj->cachehead];
        obj->cachehead = (obj->cachehead + 1) % obj->cachesz;
        obj->cachecnt--;
    } else m = obj->iavfs->alloc(obj->iavfs, &size);
    if(m && (obj->flags & DILL_ALLOC_FLAGS_ZERO))
        memset(m, 0, size);
    return m;
}

static int acache_free(struct alloc_vfs *avfs, void *p) {
    struct acache_alloc *obj =
        dill_cont(avfs, struct acache_alloc, avfs);
    if(obj->cachecnt != obj->cachesz) {
        obj->cache[obj->cachetail] = p;
        obj->cachetail = (obj->cachetail + 1) % obj->cachesz;
        obj->cachecnt++;
    } else free(p);
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
