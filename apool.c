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
#include "osalloc.h"

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
static void *apool_alloc(struct alloc_vfs *avfs);
static int apool_free(struct alloc_vfs *avfs, void *stack);
static ssize_t apool_size(struct alloc_vfs *avfs);
static int apool_caps(struct alloc_vfs *avfs);

dill_unique_id(apool_type);

struct apool_alloc {
    struct hvfs hvfs;
    struct alloc_vfs avfs;
    struct alloc_vfs *iavfs;
    size_t pgsz;
    size_t blockcount, blocktotal;
    size_t elsize, elcount, total, allocated;
    void *freelist, **blocklist;
    void **nextblock;
    int flags;
};

static void *apool_alloc_block(struct apool_alloc *obj);

static inline void *apool_hquery(struct hvfs *hvfs, const void *type) {
    struct apool_alloc *obj = (struct apool_alloc *)hvfs;
    if(type == alloc_type) return &obj->avfs;
    if(type == apool_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int apool(int flags, size_t sz, size_t count)
{
#if !HAVE_MPROTECT
    if(flags & DILL_ALLOC_FLAGS_GUARD) {errno = ENOTSUP; return -1;}
#endif
    flags |= DILL_ALLOC_FLAGS_ALIGN;
    /* Mempool allocator only works with sizes bigger than pointer size */
    if(dill_slow(sz < sizeof(void *))) sz = APOOL_DEFAULT_SIZE;
    if(dill_slow(count == 0)) count = APOOL_DEFAULT_COUNT;
    struct apool_alloc *obj = malloc(sizeof(struct apool_alloc));
    if(dill_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = apool_hquery;
    obj->hvfs.close = apool_hclose;
    obj->avfs.alloc = apool_alloc;
    obj->avfs.free = apool_free;
    obj->avfs.size = apool_size;
    obj->avfs.caps = apool_caps;
    obj->pgsz = dill_page_size();
    /* Create the block */
    obj->elsize = sz;
    /* Pick element sizes that fit the minimum requirement of flags */
    if(flags & DILL_ALLOC_FLAGS_ALIGN)
        obj->elsize = dill_align(obj->elsize, obj->pgsz);
    /* As pools are fixed-size, allocate an extra page for guards early */
    if(flags & DILL_ALLOC_FLAGS_GUARD)
        obj->elsize = dill_page_larger(obj->elsize, obj->pgsz);
    obj->elcount = count;
    obj->allocated = 0;
    obj->total = count;
    obj->flags = flags;
    void *p = apool_alloc_block(obj);
    if(!p) return -1;
    obj->freelist = p;
    /* Create the list of blocks with single item */
    size_t blocksize = obj->elsize * obj->elcount;
    obj->blocklist = dill_alloc(memalign, 1, obj->pgsz, blocksize);
    obj->nextblock = obj->blocklist + 1;
    obj->blocktotal = blocksize / sizeof(void *) - 1;
    dill_assert(obj->blocktotal > 0);
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

static void *apool_alloc_block(struct apool_alloc *obj) {
    size_t blocksize = obj->elsize * obj->elcount;
    void *p = dill_alloc(memalign, 1, obj->pgsz, blocksize);
    if(p && (obj->flags & DILL_ALLOC_FLAGS_GUARD)) {
        int i = 0;
        void *stack = p;
        while(i < obj->elcount) {
            int rc = dill_guard(stack, obj->pgsz);
            dill_assert(rc == 0);
            stack += obj->elsize;
            i++;
        }
    }
    return p;
}

static int apool_extend(struct apool_alloc *obj) {
    /* Allocate the pool block */
    void *p = apool_alloc_block(obj); 
    if(!p) return -1;
    debug("memory extension at %p\n", p);
    /* Need to allocate to the block header list too */
    if(obj->blockcount + 1 >= obj->blocktotal) {
        /* Allocate new block header extension list */
        size_t blocksize = obj->elsize * obj->elcount;
        void *b = dill_alloc(memalign, 1, obj->pgsz, blocksize);
        if(!b) return -1;
        /* Update linked list of block header extension list */
        *(void **)b = *obj->blocklist;
        *obj->blocklist = b;
        /* Update the next pointer for allocating */
        obj->nextblock = (void **)b + 1;
    }
#ifdef DILL_VALGRIND
    size_t blocksize = obj->elsize * obj->elcount;
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

static void *apool_alloc(struct alloc_vfs *avfs) {
    struct apool_alloc *obj =
        dill_cont(avfs, struct apool_alloc, avfs);
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
    return entry + obj->elsize;
}

static int apool_free(struct alloc_vfs *avfs, void *stack) {
    struct apool_alloc *obj =
        dill_cont(avfs, struct apool_alloc, avfs);
    /* Get the start of the stack memory */
    void *p = stack - obj->elsize;
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
    int caps = DILL_ALLOC_CAPS_BOOKKEEP; 
    if(obj->flags & (DILL_ALLOC_FLAGS_ZERO)) caps |= DILL_ALLOC_CAPS_ZERO;
    if(obj->flags & (DILL_ALLOC_FLAGS_ALIGN)) caps |= DILL_ALLOC_CAPS_ALIGN;
    if(obj->flags & (DILL_ALLOC_FLAGS_GUARD)) caps |= DILL_ALLOC_CAPS_GUARD;
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
                if(obj->flags & DILL_ALLOC_FLAGS_GUARD) {
                    int i = 0;
                    void *stack = *b;
                    while(i < obj->elcount) {
                        dill_unguard(stack, obj->pgsz);
                        stack += obj->elsize;
                        i++;
                    }
                }
#ifdef DILL_VALGRIND
                VALGRIND_DESTROY_MEMPOOL(b);
#endif
                free(*b);
            } else break;
        np = *p;
        free(p);
    } while(p = np);
    free(obj);
}
