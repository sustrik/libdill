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
#include <stdlib.h>

#include "cr.h"
#include "libdill.h"
#include "utils.h"

struct dill_handle {
    /* Implemetor-specified type of the handle. */
    const void *type;
    /* Opaque implemetor-specified pointer. It is set to NULL when
       handledone() is called. */
    void *data;
    /* Virtual function that gets called when the handle is being stopped. */
    hndlstop_fn stop_fn;
    /* Return value as supplied by handledone() function. */
    int result;
    /* Coroutine that performs the cancelation of this handle.
       NULL if the handle is not being canceled. */
    struct dill_cr *canceler;
    /* Index of the next handle in the linked list of unused handles. */
    int next;
};

static struct dill_handle *dill_handles = NULL;
static int dill_nhandles = 0;
static int dill_unused = -1;

int handle(const void *type, void *data, hndlstop_fn stop_fn) {
    if(dill_slow(!type || !data || !stop_fn)) {errno = EINVAL; return -1;}
    /* If there's no space for the new handle expand the array. */
    if(dill_slow(dill_unused == -1)) {
        /* Start with 256 handles, double the size when needed. */
        int sz = dill_nhandles ? dill_nhandles * 2 : 256;
        struct dill_handle *hndls =
            realloc(dill_handles, sz * sizeof(struct dill_handle));
        if(dill_slow(!hndls)) {errno = ENOMEM; return -1;}
        /* Add newly allocated handles to the list of unused handles. */
        int i;
        for(i = dill_nhandles; i != sz - 1; ++i)
            hndls[i].next = i + 1;
        hndls[sz - 1].next = -1;
        dill_unused = dill_nhandles;
        /* Adjust the array. */
        dill_handles = hndls;
        dill_nhandles = sz;
    }
    /* Return first handle from the list of unused hadles. */
    int h = dill_unused;
    dill_unused = dill_handles[h].next;
    dill_handles[h].type = type;
    dill_handles[h].data = data;
    dill_handles[h].stop_fn = stop_fn;
    dill_handles[h].result = -1;
    dill_handles[h].canceler = NULL;
    dill_handles[h].next = -1;
    /* 0 is never returned. It is reserved to refer to the main routine. */
    return h + 1;
}

const void *handletype(int h) {
    if(dill_slow(!h)) return NULL; /* TODO: return coroutine type */
    h--;
    if(dill_slow(h >= dill_nhandles || dill_handles[h].next != -1)) {
        errno = EBADF; return NULL;}
    return dill_handles[h].type;
}

void *handledata(int h) {
    if(dill_slow(!h)) return NULL; /* TODO */
    h--;
    if(dill_slow(h >= dill_nhandles || dill_handles[h].next != -1)) {
        errno = EBADF; return NULL;}
    return dill_handles[h].data;
}

int handledone(int h, int result) {
    /* Marking main routine as done has no effect. */
    if(dill_slow(!h)) return 0;
    h--;
    if(dill_slow(h >= dill_nhandles || dill_handles[h].next != -1)) {
        errno = EBADF; return -1;}
    struct dill_handle *hndl = &dill_handles[h];
    hndl->result = result;
    /* The data can be deallocated past this point. It's safer to set this
       pointer to NULL so that it's easier to debug if used by accident. */
    hndl->data = NULL;
    /* If there's stop() already waiting for this handle, resume it. */
    if(hndl->canceler)
        dill_resume(hndl->canceler, 0);
    return 0;
}

int stop(int h) {
    /* Main routine cannot be stopped. */
    if(dill_slow(h == 0)) {errno = EOPNOTSUPP; return -1;}
    --h;
    if(dill_slow(h < 0 || h >= dill_nhandles || dill_handles[h].next != -1)) {
        errno = EBADF; return -1;}
    struct dill_handle *hndl = &dill_handles[h];
    /* Stop function is called only if handledone() wasn't called before. */
    if(hndl->data) {
        /* This will guarantee that blocking functions cannot be called from
           within stop_fn. */
        int was_stopping = dill_running->stopping;
        dill_running->stopping = 1;
        /* Send stop signal to the handle. */
        dill_assert(hndl->stop_fn);
        hndl->stop_fn(h + 1);
        dill_running->stopping = was_stopping;
        /* Better be paraniod and delete the function pointer here. */
        hndl->stop_fn = NULL;
        /* Wait till cancellation finishes. */
        while(1) {
            int rc = dill_suspend(NULL);
            if(rc == 0)
                break;
            /* Owner of this coroutine stopped it. We'll ignore that. stop()
               is not strictly a blocking function and thus it is supposed to be
               immune to cancellation. */
            dill_assert(rc == -ECANCELED);
        }
    }
    /* Return the handle to the shared pool. */
    hndl->next = dill_unused;
    dill_unused = h;
    /* Return the result value provided by handledone() function. */
    return hndl->result;
}

