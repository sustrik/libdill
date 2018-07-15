/*

  Copyright (c) 2018 Martin Sustrik

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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cr.h"
#include "handle.h"
#include "utils.h"
#include "ctx.h"

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"

struct dill_handle {
    /* Table of virtual functions. */
    struct dill_hvfs *vfs;
    /* Index of the next handle in the linked list of unused handles. -1 means
       'the end of the list'. -2 means 'handle is in use'. */
    int next;
    /* Cache the last call to hquery. */
    const void *type;
    void *ptr;
};

#define DILL_CHECKHANDLE(h, err) \
    if(dill_slow((h) < 0 || (h) >= ctx->nhandles ||\
          ctx->handles[(h)].next != -2)) {\
        errno = EBADF; return (err);}\
    struct dill_handle *hndl = &ctx->handles[(h)];

int dill_ctx_handle_init(struct dill_ctx_handle *ctx) {
    ctx->handles = NULL;
    ctx->nhandles = 0;
    ctx->nused = 0;
    ctx->first = -1;
    ctx->last = -1;
    return 0;
}

void dill_ctx_handle_term(struct dill_ctx_handle *ctx) {
    free(ctx->handles);
}

int dill_hmake(struct dill_hvfs *vfs) {
    struct dill_ctx_handle *ctx = &dill_getctx->handle;
    if(dill_slow(!vfs || !vfs->query || !vfs->close)) {
        errno = EINVAL; return -1;}
    /* Returns ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* If there's no space for the new handle, expand the array.
       Keep at least 8 handles unused so that there's at least some rotation
       of handle numbers even if operating close to the current limit. */
    if(dill_slow(ctx->nhandles - ctx->nused <= 8)) {
        /* Start with 256 handles, double the size when needed. */
        int sz = ctx->nhandles ? ctx->nhandles * 2 : 256;
        struct dill_handle *hndls =
            realloc(ctx->handles, sz * sizeof(struct dill_handle));
        if(dill_slow(!hndls)) {errno = ENOMEM; return -1;}
        /* Add newly allocated handles to the list of unused handles. */
        int i;
        for(i = ctx->nhandles; i != sz - 1; ++i)
            hndls[i].next = i + 1;
        hndls[sz - 1].next = -1;
        ctx->first = ctx->nhandles;
        ctx->last = sz - 1; 
        /* Adjust the array. */
        ctx->handles = hndls;
        ctx->nhandles = sz;
    }
    /* Return first handle from the list of unused handles. */
    int h = ctx->first;
    ctx->first = ctx->handles[h].next;
    if(dill_slow(ctx->first) == -1) ctx->last = -1;
    ctx->handles[h].vfs = vfs;
    ctx->handles[h].next = -2;
    ctx->handles[h].type = NULL;
    ctx->handles[h].ptr = NULL;
    ctx->nused++;
    return h;
}

int dill_hown(int h) {
    struct dill_ctx_handle *ctx = &dill_getctx->handle;
    DILL_CHECKHANDLE(h, -1);
    /* Create a new handle for the same object. */
    int res = dill_hmake(hndl->vfs);
    if(dill_slow(res < 0)) {
        int rc = dill_hclose(h);
        dill_assert(rc == 0);
        return -1;
    }
    /* In case handle array was reallocated we have to recompute the pointer. */
    hndl = &ctx->handles[h];
    /* Return a handle to the shared pool. */
    hndl->ptr = NULL;
    hndl->next = -1;
    if(ctx->first == -1) ctx->first = h;
    else ctx->handles[ctx->last].next = h;
    ctx->last = h;
    ctx->nused--;
    return res;
}

void *dill_hquery(int h, const void *type) {
    struct dill_ctx_handle *ctx = &dill_getctx->handle;
    DILL_CHECKHANDLE(h, NULL);
    /* Try and use the cached pointer first; otherwise do the expensive virtual
       call.*/
    if(dill_fast(hndl->ptr != NULL && hndl->type == type))
        return hndl->ptr;
    else {
        void *ptr = hndl->vfs->query(hndl->vfs, type);
        if(dill_slow(!ptr)) return NULL;
        /* Update cache. */
        hndl->type = type;
        hndl->ptr = ptr;
        return ptr;
    }
}

int dill_hclose(int h) {
    struct dill_ctx_handle *ctx = &dill_getctx->handle;
    DILL_CHECKHANDLE(h, -1);
    /* This will guarantee that blocking functions cannot be called anywhere
       inside the context of the close. */
    int old = dill_no_blocking(1);
    /* Send the stop signal to the handle. */
    hndl->vfs->close(hndl->vfs);
    /* Restore the previous state. */
    dill_no_blocking(old);
    /* Mark the cache as invalid. */
    hndl->ptr = NULL;
    /* Return a handle to the shared pool. */
    hndl->next = -1;
    if(ctx->first == -1) ctx->first = h;
    else ctx->handles[ctx->last].next = h;
    ctx->last = h;
    ctx->nused--;
    return 0;
}

