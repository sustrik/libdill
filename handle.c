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
#include <list.h>
#include <stddef.h>
#include <stdlib.h>

#include "cr.h"
#include "libdill.h"
#include "utils.h"

DILL_CT_ASSERT(sizeof(struct dill_stopdata) <= DILL_OPAQUE_SIZE);

struct dill_handle {
    const void *type;
    void *data;
    /* Virtual function that gets called when the handle is being stopped.
       After it's called, this field is set to NULL. */
    hndlstop_fn stop_fn;
    int done;
    /* Coroutine that performs the cancelation of this handle.
       NULL if the handle is not being canceled. */
    struct dill_cr *canceler;
    /* Pointer to canceler's list of handles to cancel. */
    struct dill_list *tocancel;
    /* A member of the stop() function's list of handles to cancel. */
    struct dill_list_item item;
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
    dill_handles[h].done = 0;
    dill_handles[h].canceler = NULL;
    dill_handles[h].tocancel = NULL;
    dill_list_item_init(&dill_handles[h].item);
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

int handledone(int h) {
    if(dill_slow(!h)) return 0;
    h--;
    if(dill_slow(h >= dill_nhandles || dill_handles[h].next != -1)) {
        errno = EBADF; return -1;}
    struct dill_handle *hndl = &dill_handles[h];
    hndl->done = 1;
    /* The data can be deallocated past this point. It's safer to set this
       pointer to NULL so that it's easier to debug if used by accident. */
    hndl->data = NULL;
    if(!hndl->canceler)
        return 0;
    /* If there's stop() already waiting for this handle,
       remove it from the list of waiting handles and if there's no other
       handle left in the list resume the stopper. */
    dill_list_erase(hndl->tocancel, &hndl->item);
    if(dill_list_empty(hndl->tocancel))
        dill_resume(hndl->canceler, 0);
    return 0;
}

int dill_stop(int *hndls, int nhndls, int64_t deadline, const char *current) {
    dill_trace(current, "stop()");
    if(dill_slow(nhndls == 0))
        return 0;
    if(dill_slow(!hndls)) {
        errno = EINVAL;
        return -1;
    }
    /* Check whether all the handles in the array are valid. */
    int i;
    for(i = 0; i != nhndls; ++i) {
        /* Main coroutine cannot be stopped. */
        if(dill_slow(hndls[i] == 0)) {errno = EBADF; return -1;}
        int h = hndls[i] - 1;
        if(dill_slow(h >= dill_nhandles || dill_handles[h].next != -1)) {
            errno = EBADF; return -1;}
    }
    /* Set debug info. */
    dill_startop(&dill_running->debug, DILL_STOP, current);
    struct dill_stopdata *sd = (struct dill_stopdata*)dill_running->opaque;
    sd->hndls = hndls;
    sd->nhndls = nhndls;
    /* Add all not yet finished handles to a list. Let finished ones
       deallocate themselves. */
    struct dill_list tocancel;
    dill_list_init(&tocancel);
    for(i = 0; i != nhndls; ++i) {
        struct dill_handle *hndl = &dill_handles[hndls[i] - 1];
        if(hndl->done)
            continue;
        hndl->canceler = dill_running;
        hndl->tocancel = &tocancel;
        dill_list_insert(&tocancel, &hndl->item, NULL);
    }
    /* If all coroutines are finished return straight away. */
    if(dill_list_empty(&tocancel))
        goto finish;
    /* If user requested immediate cancelation or if this coroutine was already
       canceled by its owner we can skip the grace period. */
    if(deadline != 0 || dill_running->canceled || dill_running->stopping) {
      /* If required, start waiting for the timeout. */
      if(deadline > 0)
          dill_timer_add(&dill_running->timer, deadline);
      /* Wait till all coroutines are finished. */
      int rc = dill_suspend(NULL);
      if(rc == 0) {
          /* All coroutines have finished. We are done. */
          if(deadline > 0)
              dill_timer_rm(&dill_running->timer);
          goto finish;
      }
      /* We'll have to force coroutines to exit. No need for timer any more. */
      dill_assert(rc == -ECANCELED || rc == -ETIMEDOUT);
      if(rc != -ETIMEDOUT && deadline > 0)
          dill_timer_rm(&dill_running->timer);
    }
    /* Send stop signal to the remaining handles. It's tricky because sending
       the signal may result in removal of arbitrary number of handles from 
       the list. Thus the O(n^2) algorithm. It should be improved. */
    int was_stopping = dill_running->stopping;
    dill_running->stopping = 1;
    struct dill_list_item *it;
    do {
        for(it = dill_list_begin(&tocancel); it; it = dill_list_next(it)) {
            struct dill_handle *hndl = dill_cont(it, struct dill_handle, item);
            if(!hndl->stop_fn)
                continue;
            hndl->stop_fn((hndl - dill_handles) + 1);
            hndl->stop_fn = NULL;
            break;
        }
    } while(it);
    dill_running->stopping = was_stopping;
    /* Wait till they all finish. Waiting may be interrupted if this
       coroutine itself is asked to cancel itself, but there's nothing
       much to do there anyway. It'll just continue waiting. */
    while(1) {
        int rc = dill_suspend(NULL);
        if(rc == 0)
            break;
        dill_assert(rc == -ECANCELED);
    }
finish:
    /* Return all the handles back to the shared pool. */
    for(i = 0; i != nhndls; ++i) {
        int h = hndls[i] - 1;
        dill_handles[h].next = dill_unused;
        dill_unused = h;
    }
    /* if current coroutine was canceled by its owner. */
    if(dill_slow(dill_running->canceled || dill_running->stopping)) {
        errno = ECANCELED;
        return -1;
    }
    return 0;
}

