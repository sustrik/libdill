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
#include <stdio.h>

#include "cr.h"
#include "debug.h"
#include "libdill.h"
#include "poller.h"
#include "stack.h"
#include "utils.h"

DILL_CT_ASSERT(sizeof(struct dill_gocanceldata) <= DILL_OPAQUE_SIZE);

volatile int dill_unoptimisable1 = 1;
volatile void *dill_unoptimisable2 = NULL;

struct dill_cr dill_main = {DILL_SLIST_ITEM_INITIALISER};

struct dill_cr *dill_running = &dill_main;

/* Queue of coroutines scheduled for execution. */
static struct dill_slist dill_ready = {0};

#define dill_setjmp(ctx) \
    ({\
        dill_assert(!(ctx)->valid);\
        (ctx)->valid = 1;\
        sigsetjmp((ctx)->jbuf, 0);\
    })
#define dill_jmp(ctx) \
    do{\
        dill_assert((ctx)->valid);\
        (ctx)->valid = 0;\
        siglongjmp((ctx)->jbuf, 1);\
    } while(0)

int dill_suspend(dill_unblock_cb unblock_cb) {
    /* Even if process never gets idle, we have to process external events
       once in a while. The external signal may very well be a deadline or
       a user-issued command that cancels the CPU intensive operation. */
    static int counter = 0;
    if(counter >= 103) {
        dill_wait(0);
        counter = 0;
    }
    /* Store the context of the current coroutine, if any. */
    if(dill_running) {
        dill_running->unblock_cb = unblock_cb;
        if(dill_setjmp(&dill_running->ctx))
            return dill_running->result;
    }
    while(1) {
        /* If there's a coroutine ready to be executed go for it. */
        if(!dill_slist_empty(&dill_ready)) {
            ++counter;
            struct dill_slist_item *it = dill_slist_pop(&dill_ready);
            dill_running = dill_cont(it, struct dill_cr, ready);
            dill_jmp(&dill_running->ctx);
        }
        /* Otherwise, we are going to wait for sleeping coroutines
           and for external events. */
        dill_wait(1);
        dill_assert(!dill_slist_empty(&dill_ready));
        counter = 0;
    }
}

void dill_resume(struct dill_cr *cr, int result) {
    if(cr->unblock_cb) {
        cr->unblock_cb(cr);
        cr->unblock_cb = NULL;
    }
    cr->result = result;
    dill_slist_push_back(&dill_ready, &cr->ready);
}

/* The intial part of go(). Starts the new coroutine.  Returns 1 in the
   new coroutine, 0 in the old one. */
int dill_prologue(struct dill_cr **cr, const char *created) {
    /* Ensure that debug functions are available whenever a single go()
       statement is present in the user's code. */
    dill_preserve_debug();
    /* Allocate and initialise new stack. */
    *cr = ((struct dill_cr*)dill_allocstack()) - 1;
    if(dill_slow(!*cr)) {
        errno = ENOMEM;
        return 0;
    }
    dill_register_cr(&(*cr)->debug, created);
    dill_slist_item_init(&(*cr)->ready);
    (*cr)->canceled = 0;
    (*cr)->canceler = NULL;
    (*cr)->cls = NULL;
    (*cr)->unblock_cb = NULL;
    (*cr)->finished = 0;
    (*cr)->ctx.valid = 0;
    dill_trace(created, "{%d}=go()", (int)(*cr)->debug.id);
    /* Suspend the parent coroutine and make the new one running. */
    if(dill_setjmp(&dill_running->ctx))
        return 0;
    dill_resume(dill_running, 0);    
    dill_running = *cr;
    return 1;
}

/* The final part of go(). Cleans up after the coroutine is finished. */
void dill_epilogue(void) {
    dill_trace(NULL, "go() done");
    dill_startop(&dill_running->debug, DILL_FINISHED, NULL);
    dill_running->finished = 1;
    if(dill_running->canceler) {
        /* If there's gocancel() already waiting for this coroutine,
           remove it from its list and resume it if there's no other
           coroutine left it the list. */
        dill_list_erase(&dill_running->canceler->tocancel,
            &dill_running->tocancel_item);
        if(dill_list_empty(&dill_running->canceler->tocancel))
            dill_resume(dill_running->canceler, 0);
    }
    else {
        /* If gocancel() wasn't call yet wait for it. */
        int rc = dill_suspend(NULL);
        dill_assert(rc == 0);
    }
    /* Stop the coroutine and deallocate the resources. */
    dill_unregister_cr(&dill_running->debug);
    dill_freestack(dill_running + 1);
    dill_running = NULL;
    /* Given that there's no running coroutine at this point
       this call will never return. */
    dill_suspend(NULL);
}

int dill_yield(const char *current) {
    dill_trace(current, "yield()");
    if(dill_slow(dill_running->canceled)) {
        errno = ECANCELED;
        return -1;
    }
    dill_startop(&dill_running->debug, DILL_YIELD, current);
    /* This looks fishy, but yes, we can resume the coroutine even before
       suspending it. */
    dill_resume(dill_running, 0);
    int rc = dill_suspend(NULL);
    if(rc == 0)
        return 0;
    dill_assert(rc < 0);
    errno = -rc;
    return -1;
}

int dill_gocancel(coro *crs, int ncrs, int64_t deadline, const char *current) {
    dill_trace(current, "gocancel()");
    if(dill_slow(ncrs == 0))
        return 0;
    if(dill_slow(!crs)) {
        errno = EINVAL;
        return -1;
    }
    /* Set debug info. */
    dill_startop(&dill_running->debug, DILL_GOCANCEL, current);
    struct dill_gocanceldata *gcd =
        (struct dill_gocanceldata*)dill_running->opaque;
    gcd->crs = crs;
    gcd->ncrs = ncrs;
    /* Add all not yet finished coroutines to a list. Let finished ones
       deallocate themselves. */
    dill_list_init(&dill_running->tocancel);
    int i;
    for(i = 0; i != ncrs; ++i) {
        if(crs[i]->finished) {
            dill_resume(crs[i], 0);
            continue;
        }
        crs[i]->canceler = dill_running;
        dill_list_insert(&dill_running->tocancel, &crs[i]->tocancel_item, NULL);
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

void *cls(void) {
    return dill_running->cls;
}

void setcls(void *val) {
    dill_running->cls = val;
}

