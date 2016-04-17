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

#if defined DILL_VALGRIND
#include <valgrind/valgrind.h>
#endif

#include "cr.h"
#include "poller.h"
#include "utils.h"

/* Currently running coroutine. */
static struct dill_cr *dill_r;

/* List of coroutines ready for execution. */
struct dill_slist dill_ready = {0};

void dill_waitfor(struct dill_clause *cl, int id) {
    cl->cr = dill_r;
    dill_slist_push_back(&dill_r->clauses, &cl->item);
    cl->id = id;
}

int dill_wait(void)  {
    /* Even if process never gets idle, we have to process external events
       once in a while. The external signal may very well be a deadline or
       a user-issued command that cancels the CPU intensive operation. */
    static int counter = 0;
    /* 103 is a prime. That way it's less likely to coincide with some kind
       of cycle in the user's code. */
    if(counter >= 103) {
        dill_poller_wait(0);
        counter = 0;
    }
    /* Store the context of the current coroutine, if any. */
    if(dill_r) {
        if(sigsetjmp(dill_r->ctx,0)) {
            /* We get here once the coroutine is resumed. */
            errno = dill_r->err;
            return dill_r->id;
        }
    }
    while(1) {
        /* If there's a coroutine ready to be executed jump to it. */
        if(!dill_slist_empty(&dill_ready)) {
            ++counter;
            struct dill_slist_item *it = dill_slist_pop(&dill_ready);
            dill_r = dill_cont(it, struct dill_cr, ready);
            siglongjmp(dill_r->ctx, 1);
        }
        /* Otherwise, we are going to wait for sleeping coroutines
           and for external events. */
        dill_poller_wait(1);
        /* Sanity check: External events must have unblocked at least
           one coroutine. */
        dill_assert(!dill_slist_empty(&dill_ready));
        counter = 0;
    }
}

static void dill_docancel(struct dill_cr *cr) {
    /* Sanity check: Make sure that the coroutine was really suspended. */
    dill_assert(!dill_slist_item_inlist(&cr->ready));
    /* Remove the clauses from endpoints' lists of waiting coroutines. */
    struct dill_slist_item *it;
    for(it = dill_slist_begin(&cr->clauses); it; it = dill_slist_next(it)) {
        struct dill_clause *cl = dill_cont(it, struct dill_clause, item);
        if(cl->eplist)
            dill_list_erase(cl->eplist, &cl->epitem);
    }
    /* Schedule the newly unblocked coroutine for execution. */
    dill_slist_push_back(&dill_ready, &cr->ready);
}

void dill_trigger(struct dill_clause *cl, int err) {
    struct dill_cr *cr = cl->cr;
    cr->id = cl->id;
    cr->err = err;
    dill_docancel(cr);
}

void dill_cancel(struct dill_cr *cr, int err) {
    cr->id = -1;
    cr->err = err;
    dill_docancel(cr);
}

