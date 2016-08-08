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
#include <setjmp.h>

#if defined DILL_VALGRIND
#include <valgrind/valgrind.h>
#endif

#include "cr.h"
#include "fd.h"
#include "libdill.h"
#include "pollset.h"
#include "stack.h"
#include "utils.h"

/* The coroutine. The memory layout looks like this:
   +-------------------------------------------------------------+---------+
   |                                                      stack  | dill_cr |
   +-------------------------------------------------------------+---------+
   - dill_cr contains generic book-keeping info about the coroutine
   - stack is a standard C stack; it grows downwards (at the moment libdill
     doesn't support microarchitectures where stack grows upwards)
*/
struct dill_cr {
    /* When coroutine is ready for execution but not running yet,
       it lives in this list (dill_ready). 'id' is a result value to return
       from dill_wait() once the coroutine is resumed. Additionally, errno
       will be set to value of 'err'. */
    struct dill_slist_item ready;
    int id;
    int err;
    /* When coroutine is suspended 'ctx' holds the context
       (registers and such). */
    sigjmp_buf ctx;
    /* If coroutine is blocked, here's the list of clauses it waits for. */
    struct dill_slist clauses;
    /* Coroutine-local storage. */
    void *cls;
    /* There are two possible reasons to disable blocking calls.
       1. The coroutine is being closed by its owner.
       2. The execution is happenin within a context of hclose() function. */
    unsigned int no_blocking1 : 1;
    unsigned int no_blocking2 : 1;
    /* Set once coroutine has finished its execution. */
    unsigned int done : 1;
    /* When coroutine handle is being closed, this is the pointer to the
       coroutine that is doing the hclose() call. */
    struct dill_cr *closer;
#if defined DILL_VALGRIND
    /* Valgrind stack identifier. This way valgrind knows which areas of
       memory are used as a stacks and doesn't produce spurious warnings.
       Well, sort of. The mechanism is not perfect, but it's still better
       than nothing. */
    int sid;
#endif
};

/* Storage for constants used by go() macro. */
volatile int dill_unoptimisable1 = 1;
volatile void *dill_unoptimisable2 = NULL;

/* Main coroutine. */
struct dill_cr dill_main_data = {DILL_SLIST_ITEM_INITIALISER};
struct dill_cr *dill_main = &dill_main_data;

/* Currently running coroutine. */
struct dill_cr *dill_r = &dill_main_data;

/* List of coroutines ready for execution. */
struct dill_slist dill_ready = {0};

/* 1 if dill_poller_init() was already run. */
static int dill_poller_initialised = 0;

/* Global linked list of all timers. The list is ordered.
   First timer to be resumed comes first and so on. */
static struct dill_list dill_timers;

/******************************************************************************/
/*  Helpers.                                                                  */
/******************************************************************************/

static void dill_resume(struct dill_cr *cr, int id, int err) {
    cr->id = id;
    cr->err = err;
    dill_slist_push_back(&dill_ready, &cr->ready);
}

int dill_canblock(void) {
    if(dill_r->no_blocking1 || dill_r->no_blocking2) {
        errno = ECANCELED;
        return -1;
    }
    return 0;
}

int dill_no_blocking2(int val) {
    int old = dill_r->no_blocking2;
    dill_r->no_blocking2 = val;
    return old;
}

/******************************************************************************/
/*  Poller.                                                                   */
/******************************************************************************/

static void dill_poller_init(int parent) {
    /* If intialisation was already done, do nothing. */
    if(dill_fast(dill_poller_initialised)) return;
    dill_poller_initialised = 1;
    /* Timers. */
    dill_list_init(&dill_timers);
    /* Polling-mechanism-specific intitialisation. */
    int rc = dill_pollset_init(parent);
    dill_assert(rc == 0);
}

static void dill_poller_term(void) {
    dill_poller_initialised = 0;
    /* Polling-mechanism-specific termination. */
    dill_pollset_term();
    /* Get rid of all the timers inherited from the parent. */
    dill_list_init(&dill_timers);
}

/* Adds a timer clause to the list of waited for clauses. */
void dill_timer(struct dill_tmcl *tmcl, int id, int64_t deadline) {
    dill_poller_init(-1);
    /* If the deadline is infinite there's nothing to wait for. */
    if(deadline < 0) return;
    /* Finite deadline. */
    tmcl->deadline = deadline;
    /* Move the timer into the right place in the ordered list
       of existing timers. TODO: This is an O(n) operation! */
    struct dill_list_item *it = dill_list_begin(&dill_timers);
    while(it) {
        struct dill_tmcl *itcl = dill_cont(it, struct dill_tmcl, cl.epitem);
        /* If multiple timers expire at the same momemt they will be fired
           in the order they were created in (> rather than >=). */
        if(itcl->deadline > tmcl->deadline)
            break;
        it = dill_list_next(it);
    }
    dill_waitfor(&tmcl->cl, id, &dill_timers, it);
}

int dill_in(struct dill_clause *cl, int id, int fd) {
    dill_poller_init(-1);
    if(dill_slow(fd < 0 || fd >= dill_maxfds())) {errno = EBADF; return -1;}
    return dill_pollset_in(cl, id, fd);
}

int dill_out(struct dill_clause *cl, int id, int fd) {
    dill_poller_init(-1);
    if(dill_slow(fd < 0 || fd >= dill_maxfds())) {errno = EBADF; return -1;}
    return dill_pollset_out(cl, id, fd);
}

void dill_clean(int fd) {
    dill_poller_init(-1);
    dill_pollset_clean(fd);
}

/* Wait for external events such as timers or file descriptors. If block is set
   to 0 the function will poll for events and return immediately. If it is set
   to 1 it will block until there's at least one event to process. */
static void dill_poller_wait(int block) {
    dill_poller_init(-1);
    while(1) {
        /* Compute timeout for the subsequent poll. */
        int timeout = 0;
        if(block) {
            if(dill_list_empty(&dill_timers))
                timeout = -1;
            else {
                int64_t nw = now();
                int64_t deadline = dill_cont(dill_list_begin(&dill_timers),
                    struct dill_tmcl, cl.epitem)->deadline;
                timeout = (int) (nw >= deadline ? 0 : deadline - nw);
            }
        }
        /* Wait for events. */
        int fired = dill_pollset_poll(timeout);
        /* Fire all expired timers. */
        if(!dill_list_empty(&dill_timers)) {
            int64_t nw = now();
            while(!dill_list_empty(&dill_timers)) {
                struct dill_tmcl *tmcl = dill_cont(
                    dill_list_begin(&dill_timers), struct dill_tmcl, cl.epitem);
                if(tmcl->deadline > nw)
                    break;
                dill_list_erase(&dill_timers, dill_list_begin(&dill_timers));
                dill_trigger(&tmcl->cl, ETIMEDOUT);
                fired = 1;
            }
        }
        /* Never retry the poll in non-blocking mode. */
        if(!block || fired)
            break;
        /* If timeout was hit but there were no expired timers do the poll
           again. It can happen if the timers were canceled in the meantime. */
    }
}

/******************************************************************************/
/*  Handle implementation.                                                    */
/******************************************************************************/

static const int dill_cr_type_placeholder = 0;
static const void *dill_cr_type = &dill_cr_type_placeholder;
static void dill_cr_close(int h);
static const struct hvfptrs dill_cr_vfptrs = {dill_cr_close};

/******************************************************************************/
/*  Creation and termination of coroutines.                                   */
/******************************************************************************/

static void dill_cancel(struct dill_cr *cr, int err);

/* The intial part of go(). Allocates a new stack and handle. */
int dill_prologue(sigjmp_buf **ctx) {
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) {errno = ECANCELED; return -1;}
    /* Allocate and initialise new stack. */
    size_t stacksz;
    struct dill_cr *cr = (struct dill_cr*)dill_allocstack(&stacksz);
    if(dill_slow(!cr)) return -1;
    --cr;
    int hndl = handle(dill_cr_type, cr, &dill_cr_vfptrs);
    if(dill_slow(hndl < 0)) {dill_freestack(cr); errno = ENOMEM; return -1;}
    dill_slist_item_init(&cr->ready);
    dill_slist_init(&cr->clauses);
    cr->closer = NULL;
    cr->cls = NULL;
    cr->no_blocking1 = 0;
    cr->no_blocking2 = 0;
    cr->done = 0;
#if defined DILL_VALGRIND
    cr->sid = VALGRIND_STACK_REGISTER((char*)(cr + 1) - stacksz, cr);
#endif
    /* Return the context of the parent coroutine to the caller so that it can
       store its current state. It can't be done here becuse we are at the
       wrong stack frame here. */
    *ctx = &dill_r->ctx;
    /* Add parent coroutine to the list of coroutines ready for execution. */
    dill_resume(dill_r, 0, 0);
    /* Mark the new coroutine as running. */
    dill_r = cr;
    return hndl;
}

/* The final part of go(). Gets called one the coroutine is finished. */
void dill_epilogue(void) {
    /* Mark the coroutine as finished. */
    dill_r->done = 1;
    /* If there's a coroutine waiting till we finish, unblock it now. */
    if(dill_r->closer)
        dill_cancel(dill_r->closer, 0);
    /* With no clauses added, this call will never return. */
    dill_assert(dill_slist_empty(&dill_r->clauses));
    dill_wait();
}

/* Gets called when coroutine handle is closed. */
static void dill_cr_close(int h) {
    struct dill_cr *cr = (struct dill_cr*)hdata(h, dill_cr_type);
    /* If the coroutine have already finished, we are done. */
    if(!cr->done) {
        /* No blocking calls from this point on. */
        cr->no_blocking1 = 1;
        /* Resume the coroutine if it was blocked. */
        if(!dill_slist_item_inlist(&cr->ready))
            dill_cancel(cr, ECANCELED);
        /* Wait till the coroutine finishes execution. With no clauses added
           the only mechanism to resume is dill_cancel(). This is not really
           a blocking call although it looks like one. Given that the coroutine
           that is being shut down is not permitted to block we should get
           control back pretty quickly. */
        cr->closer = dill_r;
        int rc = dill_wait();
        dill_assert(rc == -1 && errno == 0);
    }
    /* Now that the coroutine is finished deallocate it. */
    dill_freestack(cr + 1);
}

void dill_shutdown(void) {
    dill_main->no_blocking1 = 1;
    if(!dill_slist_item_inlist(&dill_main->ready))
        dill_cancel(dill_main, ECANCELED);
}

void dill_postfork(int parent) {
    /* Currently running coroutine will become the new main. */
    dill_main = dill_r;
    /* Ignore all the scheduled coroutines. */
    dill_slist_init(&dill_ready);
    /* Re-create the polling infrastructure. */
    dill_poller_term();
    dill_poller_init(parent);
    /* TODO: Re-create the stack cache. */
}

/******************************************************************************/
/*  Suspend/resume functionality.                                             */
/******************************************************************************/

void dill_waitfor(struct dill_clause *cl, int id,
      struct dill_list *eplist, struct dill_list_item *before) {
    /* Add the clause to the endpoint's list of waiting clauses. */
    dill_list_item_init(&cl->epitem);
    dill_list_insert(eplist, &cl->epitem, before);
    cl->eplist = eplist;
    /* Add clause to the coroutine list of active clauses. */
    cl->cr = dill_r;
    dill_slist_item_init(&cl->item);
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
        if(dill_setjmp(dill_r->ctx)) {
            /* We get here once the coroutine is resumed. */
            dill_slist_init(&dill_r->clauses);
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
            dill_longjmp(dill_r->ctx);
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

static void dill_docancel(struct dill_cr *cr, int id, int err) {
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
    dill_resume(cr, id, err);
}

void dill_trigger(struct dill_clause *cl, int err) {
    dill_docancel(cl->cr, cl->id, err);
}

static void dill_cancel(struct dill_cr *cr, int err) {
    dill_docancel(cr, -1, err);
}

int yield(void) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Put the current coroutine into the ready queue. */
    dill_resume(dill_r, 0, 0);
    /* Suspend. */
    return dill_wait();
}

/******************************************************************************/
/*  Coroutine-local storage.                                                  */
/******************************************************************************/

void *cls(void) {
    return dill_r->cls;
}

void setcls(void *val) {
    dill_r->cls = val;
}

