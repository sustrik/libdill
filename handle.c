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

DILL_CT_ASSERT(sizeof(struct dill_stopdata) <= DILL_OPAQUE_SIZE);

struct dill_handle {
    void *type;
    void *data;
    hndlstop_fn stop;
    int done;
    int next;
};

static struct dill_handle *dill_handles = NULL;
static int dill_nhandles = 0;
static int dill_unused = -1;

int handle(void *type, void *data, hndlstop_fn stop) {
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
    dill_handles[h].stop = stop;
    dill_handles[h].done = 0;
    dill_handles[h].next = -1;
    /* 0 is never returned. It is reserved to refer to the main routine. */
    return h + 1;
}

void *handletype(int h) {
    if(dill_slow(!h)) return NULL;
    h--;
    if(dill_slow(h >= dill_nhandles || dill_handles[h].next != -1)) {
        errno = EBADF; return NULL;}
    return dill_handles[h].type;
}

void *handledata(int h) {
    if(dill_slow(!h)) return NULL;
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
    dill_handles[h].done = 1;
    return 0;
}

int handleclose(int h) {
    if(dill_slow(!h)) {errno = EINVAL; return -1;}
    h--;
    if(dill_slow(h >= dill_nhandles || dill_handles[h].next != -1)) {
        errno = EBADF; return -1;}
    /* Return the handle to the list of unused handles. */
    dill_handles[h].next = dill_unused;
    dill_unused = h;
    return 0;
}

int dill_stop(int *hndls, int nhndls, int64_t deadline,
      const char *current) {
    dill_trace(current, "stop()");
    if(dill_slow(nhndls == 0))
        return 0;
    if(dill_slow(!hndls)) {
        errno = EINVAL;
        return -1;
    }
    /* Set debug info. */
    dill_startop(&dill_running->debug, DILL_STOP, current);
    struct dill_stopdata *sd = (struct dill_stopdata*)dill_running->opaque;
    sd->hndls = hndls;
    sd->nhndls = nhndls;
    /* Add all not yet finished coroutines to a list. Let finished ones
       deallocate themselves. */
    dill_list_init(&dill_running->tocancel);
    int i;
    for(i = 0; i != nhndls; ++i) {
        struct dill_cr *cr = (struct dill_cr*)handledata(hndls[i]);
        if(dill_handles[cr->hndl - 1].done) {
            dill_resume(cr, 0);
            continue;
        }
        cr->canceler = dill_running;
        dill_list_insert(&dill_running->tocancel, &cr->tocancel_item, NULL);
    }
    /* If all coroutines are finished return straight away. */
    int canceled = dill_running->canceled;
    if(dill_list_empty(&dill_running->tocancel))
        goto finish;
    /* If user requested immediate cancelation or if this coroutine was already
       canceled by its owner we can skip the grace period. */
    if(deadline != 0 || dill_running->canceled) {
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
      if(rc == -ECANCELED)
          canceled = 1;
    }
    /* Send cancel signal to the remaining coroutines. */
    struct dill_list_item *it;
    for(it = dill_list_begin(&dill_running->tocancel); it;
          it = dill_list_next(it)) {
        struct dill_cr *cr = dill_cont(it, struct dill_cr, tocancel_item);
        cr->canceled = 1;
        if(!dill_slist_item_inlist(&cr->ready))
            dill_resume(cr, -ECANCELED);
    }
    /* Wait till they all finish. Waiting may be interrupted if this
       coroutine itself is asked to cancel itself, but there's nothing
       much to do there anyway. It'll just continue waiting. */
    while(1) {
        int rc = dill_suspend(NULL);
        if(rc == 0)
            break;
        dill_assert(rc == -ECANCELED);
        canceled = 1;
    }
finish:
    if(canceled) {
        /* This coroutine was canceled by its owner. */
        errno = ECANCELED;
        return -1;
    }
    return 0;
}

