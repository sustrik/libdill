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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cr.h"
#include "handle.h"
#include "libdill.h"
#include "utils.h"
#include "ctx.h"

struct dill_handle {
    /* Table of virtual functions. */
    struct hvfs *vfs;
    /* Index of the next handle in the linked list of unused handles. -1 means
       'end of the list'. -2 means 'handle is in use'. */
    int next;
    /* Cache hquery's last call. */
    const void *type;
    void *ptr;
};

#define CHECKHANDLE(h, err) \
    if(dill_slow((h) < 0 || (h) >= ctx->nhandles ||\
          ctx->handles[(h)].next != -2)) {\
        errno = EBADF; return (err);}\
    struct dill_handle *hndl = &ctx->handles[(h)];

int dill_ctx_handle_init(struct dill_ctx_handle *ctx) {
    ctx->handles = NULL;
    ctx->nhandles = 0;
    ctx->unused = -1;
    return 0;
}

void dill_ctx_handle_term(struct dill_ctx_handle *ctx) {
    free(ctx->handles);
}

int hmake(struct hvfs *vfs) {
    struct dill_ctx_handle *ctx = &dill_getctx->handle;
    if(dill_slow(!vfs || !vfs->query || !vfs->close)) {
        errno = EINVAL; return -1;}
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* If there's no space for the new handle expand the array. */
    if(dill_slow(ctx->unused == -1)) {
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
        ctx->unused = ctx->nhandles;
        /* Adjust the array. */
        ctx->handles = hndls;
        ctx->nhandles = sz;
    }
    /* Return first handle from the list of unused hadles. */
    int h = ctx->unused;
    ctx->unused = ctx->handles[h].next;
    vfs->refcount = 1;
    ctx->handles[h].vfs = vfs;
    ctx->handles[h].next = -2;
    ctx->handles[h].type = NULL;
    ctx->handles[h].ptr = NULL;
    return h;
}

int hdup(int h) {
    struct dill_ctx_handle *ctx = &dill_getctx->handle;
    CHECKHANDLE(h, -1);
    int refcount = hndl->vfs->refcount;
    int res = hmake(hndl->vfs);
    if(dill_slow(res < 0)) return -1;
    hndl->vfs->refcount = refcount + 1;
    return res;
}

void *hquery(int h, const void *type) {
    struct dill_ctx_handle *ctx = &dill_getctx->handle;
    CHECKHANDLE(h, NULL);
    /* Try and use cached pointer first, otherwise do expensive virtual call.*/
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

int hclose(int h) {
    struct dill_ctx_handle *ctx = &dill_getctx->handle;
    CHECKHANDLE(h, -1);
    /* If there are multiple duplicates of this handle just remove one
       reference. */
    if(hndl->vfs->refcount > 1) {
        --hndl->vfs->refcount;
        return 0;
    }
    /* This will guarantee that blocking functions cannot be called anywhere
       inside the context of the close. */
    int old = dill_no_blocking2(1);
    /* Send stop signal to the handle. */
    hndl->vfs->close(hndl->vfs);
    /* Restore the previous state. */
    dill_no_blocking2(old);
    /* Mark the cache as invalid. */
    hndl->ptr = NULL;
    /* Return the handle to the shared pool. */
    hndl->next = ctx->unused;
    ctx->unused = h;
    return 0;
}

