/*

  Copyright (c) 2016 Martin Sustrik
  Copyright (c) 2016 Tai Chi Minh Ralph Eastwood

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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef DILL_VALGRIND
#include <valgrind/memcheck.h>
#endif

#include "utils.h"
#include "libdill.h"

/* This allocator tracks:

   - pool items allocated
   - list of pool blocks (which contain the pool items)
   - list of block header extensions (which contain block list metadata)

   The reason for the separation is to map neatly to an underlying allocator
   which has fixed size allocation without significantly wasting space per
   allocation.  Thus, the metadata is stored separately to the pool blocks which
   then can be completely filled.

   TODO: As the underlying allocation mechanism may over-allocate memory, one could
   fit more memory blocks in.  However, this has not been implemented yet.
*/

//#define DILL_APOOL_DEBUG
#ifdef DILL_APOOL_DEBUG
#include <stdio.h>
#define debug(f,...) fprintf(stderr, "%s: " f, __func__, __VA_ARGS__)
#else
#define debug(...)
#endif

#define APOOL_DEFAULT_COUNT 1024
#define APOOL_DEFAULT_SIZE (32 * 1024)

static inline void *apool_hquery(struct hvfs *hvfs, const void *type);
static void apool_hclose(struct hvfs *hvfs);
static void *apool_alloc(struct alloc_vfs *avfs, size_t *sz);
static int apool_free(struct alloc_vfs *avfs, void *p);
static ssize_t apool_size(struct alloc_vfs *avfs);
static int apool_caps(struct alloc_vfs *avfs);

dill_unique_id(apool_type);

struct apool_alloc {
    struct hvfs hvfs;
    struct alloc_vfs avfs;
    struct alloc_vfs *iavfs;
    size_t blockcount, blocktotal;
    size_t elsize, elcount, total, allocated;
    void *freelist, **blocklist;
    void **nextblock;
    int flags;
    int needzero;
};

static inline void *apool_hquery(struct hvfs *hvfs, const void *type) {
    struct apool_alloc *obj = (struct apool_alloc *)hvfs;
    if(type == alloc_type) return &obj->avfs;
    if(type == apool_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int apool(int a, int flags, size_t sz, size_t count)
{
    /* Mempool allocator only works with sizes bigger than pointer size */
    if(dill_slow(sz < sizeof(void *))) sz = APOOL_DEFAULT_SIZE;
    if(dill_slow(count == 0)) count = APOOL_DEFAULT_COUNT;
    if(flags & DILL_ALLOC_FLAGS_GUARD) {errno = ENOTSUP; return -1;}
    if(flags & DILL_ALLOC_FLAGS_HUGE) {errno = ENOTSUP; return -1;}
    /* Get underlying allocator type */
    struct alloc_vfs *iavfs = hquery(a, alloc_type);
    /* Check whether underlying type is an allocator */
    if(dill_slow(!iavfs)) return -1;
    /* Check that the size underlying allocator can fit a pool block */
    if(dill_slow(!(iavfs->caps(iavfs) & DILL_ALLOC_CAPS_RESIZE)))
        if(sz * count > iavfs->size(iavfs)) {errno = ENOTSUP; return -1;}
    /* The guard page will interfere with the pool */
    if(dill_slow(iavfs->caps(iavfs) & DILL_ALLOC_CAPS_GUARD))
        {errno = ENOTSUP; return -1;}
    struct apool_alloc *obj = malloc(sizeof(struct apool_alloc));
    if(dill_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = apool_hquery;
    obj->hvfs.close = apool_hclose;
    obj->avfs.alloc = apool_alloc;
    obj->avfs.free = apool_free;
    obj->avfs.size = apool_size;
    obj->avfs.caps = apool_caps;
    obj->iavfs = iavfs;
    /* Create the block */
    size_t blocksize = sz * count;
    obj->elsize = sz;
    obj->elcount = count;
    obj->allocated = 0;
    obj->total = count;
    void *p = iavfs->alloc(iavfs, &blocksize);
    if(!p) {return -1;}
    obj->freelist = p;
    /* Create the list of blocks with single item */
    obj->blocklist = iavfs->alloc(iavfs, &blocksize);
    obj->nextblock = obj->blocklist + 1;
    obj->blocktotal = (sz * count) / sizeof(void *) - 1;
    dill_assert(obj->blocktotal > 0);
    obj->needzero = !!(iavfs->caps(iavfs) & DILL_ALLOC_CAPS_ZERO);
    if(obj->needzero) {
        memset(p, 0, blocksize);
        memset(obj->blocklist, 0, blocksize);
    }
#ifdef DILL_VALGRIND
    debug("memblock => %lp\n", obj->nextblock);
    VALGRIND_CREATE_MEMPOOL(obj->nextblock, 0,
        !!(flags & DILL_ALLOC_FLAGS_ZERO));
    VALGRIND_MAKE_MEM_NOACCESS(p, blocksize);
#endif
    *obj->nextblock++ = p;
    obj->blockcount = 1;
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

#ifdef DILL_VALGRIND
static void *apool_findblock(struct apool_alloc *obj, void *entry) {
    void **b, **p = obj->blocklist;
    size_t blocksize = obj->elsize * obj->elcount;
    size_t blockents = blocksize / sizeof(void *) - 1;
    do {
        size_t count;
        for(b = p + 1, count = 0; count < blockents; b++, count++) {
            if(!*b) break;
            if(*b <= entry && (entry - *b) <= blocksize)
                return b;
        }
    } while(p = *p);
    return NULL;
}
#endif

static int apool_extend(struct apool_alloc *obj) {
    size_t blocksize = obj->elsize * obj->elcount;
    /* Allocate the pool block */
    void *p = obj->iavfs->alloc(obj->iavfs, &blocksize);
    if(!p) return -1;
    if(obj->needzero) memset(p, 0, blocksize);
    debug("memory extension at %p\n", p);
    /* Need to allocate to the block header list too */
    if(obj->blockcount + 1 >= obj->blocktotal) {
        /* Allocate new block header extension list */
        void *b = obj->iavfs->alloc(obj->iavfs, &blocksize);
        if(!b) return -1;
        if(obj->needzero) memset(b, 0, blocksize);
        /* Update linked list of block header extension list */
        *(void **)b = *obj->blocklist;
        *obj->blocklist = b;
        /* Update the next pointer for allocating */
        obj->nextblock = (void **)b + 1;
    }
#ifdef DILL_VALGRIND
    VALGRIND_CREATE_MEMPOOL(obj->nextblock, 0,
        !!(obj->flags & DILL_ALLOC_FLAGS_ZERO));
    VALGRIND_MAKE_MEM_NOACCESS(p, blocksize);
#endif
    obj->total += obj->elcount;
    obj->blockcount++;
    *obj->nextblock++ = p;
    obj->freelist = p;
    obj->blocktotal += (obj->elsize * obj->elcount) / sizeof(void *) - 1;
    return 0;
}

static void *apool_alloc(struct alloc_vfs *avfs, size_t *sz) {
    struct apool_alloc *obj =
        dill_cont(avfs, struct apool_alloc, avfs);
    /* Always use the size given in the pool */
    if(*sz > obj->elsize) {errno = ENOTSUP; return NULL;}
    *sz = obj->elsize;
    debug("start => allocated = %lu, total = %lu\n", obj->allocated, obj->total);
    /* Add an extension to the memory pool */
    if(dill_slow(obj->allocated >= obj->total) && apool_extend(obj) == -1)
            return NULL;
    /* If there is another pointer in the free list
       update the free list start pointer
       otherwise look for the next entry block */
#ifdef DILL_VALGRIND
    /* Need to find the pool block that this entry belongs to for valgrind */
    void *b = apool_findblock(obj, obj->freelist);
    dill_assert(b);
    debug("memblock => %lp alloc = %lp\n", b, obj->freelist);
    VALGRIND_MEMPOOL_ALLOC(b, obj->freelist, obj->elsize);
#endif
    void *entry = obj->freelist;
    if(dill_slow(!entry)) return NULL;
    void *nextentry = *(void **)entry;
    obj->freelist = nextentry ? nextentry : (char *)entry + obj->elsize;
    obj->allocated++;
    if(obj->flags & DILL_ALLOC_FLAGS_ZERO) memset(entry, 0, obj->elsize);
    debug("end => allocated = %lu, total = %lu, address = %p\n",
            obj->allocated, obj->total, entry);
    return entry;
}

static int apool_free(struct alloc_vfs *avfs, void *p) {
    struct apool_alloc *obj =
        dill_cont(avfs, struct apool_alloc, avfs);
    /* Store at the location of the freed memory, the pointer to the
       next element in the free list */
    *(void **)p = obj->freelist;
    /* Point the free list to p */
    obj->freelist = p;
    obj->allocated--;
#ifdef DILL_VALGRIND
    /* Need to find the pool block that this entry belongs to for valgrind */
    void *b = apool_findblock(obj, p);
    dill_assert(b);
    VALGRIND_MEMPOOL_FREE(b, p);
#endif
    debug("free => allocated = %lu, total = %lu, address = %p\n",
       obj->allocated, obj->total, p);
    return 0;
}

static ssize_t apool_size(struct alloc_vfs *avfs) {
    struct apool_alloc *obj =
        dill_cont(avfs, struct apool_alloc, avfs);
    return obj->elsize;
}

static int apool_caps(struct alloc_vfs *avfs) {
    struct apool_alloc *obj =
        dill_cont(avfs, struct apool_alloc, avfs);
    int caps = 0; 
    if(obj->flags & (DILL_ALLOC_FLAGS_ZERO)) caps |= DILL_ALLOC_CAPS_ZERO;
    return caps;
}

static void apool_hclose(struct hvfs *hvfs) {
    struct apool_alloc *obj = (struct apool_alloc *)hvfs;
    void **p = obj->blocklist, *np;
    void **b;
    size_t count;
    size_t blocksize = obj->elsize * obj->elcount;
    size_t blockents = blocksize / sizeof(void *) - 1;
    dill_assert(p);
    do {
        for(b = p + 1, count = 0; count < blockents; b++, count++)
            if(*b) {
#ifdef DILL_VALGRIND
                VALGRIND_DESTROY_MEMPOOL(b);
#endif
                obj->iavfs->free(obj->iavfs, *b);
            } else break;
        np = *p;
        obj->iavfs->free(obj->iavfs, p);
    } while(p = np);
    free(obj);
}
