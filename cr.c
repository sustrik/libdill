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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined DILL_VALGRIND
#include <valgrind/valgrind.h>
#endif

#include "cr.h"
#include "fd.h"
#include "libdill.h"
#include "pollset.h"
#include "stack.h"
#include "utils.h"
#include "context.h"

#if defined DILL_CENSUS

/* When doing stack size census we will keep maximum stack size in a list
   indexed by go() call, i.e. by file name and line number. */
struct dill_census_item {
    struct dill_slist_item crs;
    const char *file;
    int line;
    size_t max_stack;
};

#endif

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
    /* Virtual function table. */
    struct hvfs vfs;
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
    /* If true, the coroutine was launched via go_stack. */
    unsigned int go_stack : 1;
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
#if defined DILL_CENSUS
    /* Census record corresponding to this coroutine. */
    struct dill_census_item *census;
    size_t stacksz;
#endif
/* Clang assumes that the client stack is aligned to 16-bytes on x86-64
   architectures; to achieve this we align this structure (with the added
   benefit of a minor optimisation). */
} __attribute__((aligned(16)));

/* Storage for constant used by go() macro. */
volatile void *dill_unoptimisable = NULL;

/* Main coroutine. */
static DILL_THREAD_LOCAL struct dill_cr dill_main_data =
    {DILL_SLIST_ITEM_INITIALISER};

/* Current coroutine context */
struct dill_ctx_cr {
    /* Currently running coroutine. */
    struct dill_cr *r;
    /* List of coroutines ready for execution. */
    struct dill_slist ready;
    /* 1 if dill_poller_init() was already run. */
    int poller_init;
    /* Global linked list of all timers. The list is ordered.
       First timer to be resumed comes first and so on. */
    struct dill_list timers;
    int wait_counter;
#if defined DILL_CENSUS
    int census_init;
    struct dill_slist census;
#endif
#if defined DILL_THREADS
    int initialized;
#endif
};

/******************************************************************************/
/*  Helpers.                                                                  */
/******************************************************************************/

static struct dill_ctx_cr *dill_cr_init(void) {
    if(dill_slow(!dill_context.cr)) {
        dill_context.cr = calloc(sizeof(struct dill_ctx_cr), 1);
        dill_context.cr->r = &dill_main_data;
    }
    return dill_context.cr;
}

#if defined DILL_THREADS
static void dill_cr_atexit(void) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    free(ctx);
}
#endif

#if defined DILL_CENSUS
/* Print out the results of the stack size census. */
static void dill_census_atexit(void) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    struct dill_slist_item *it;
    for(it = dill_slist_begin(&ctx->census); it; it = dill_slist_next(it)) {
        struct dill_census_item *ci =
            dill_cont(it, struct dill_census_item, crs);
        fprintf(stderr, "%s:%d - maximum stack size %zu B\n",
            ci->file, ci->line, ci->max_stack);
    }
}
#endif

#if defined(DILL_VALGRIND) || defined(DILL_THREADS)
static void dill_pollset_atexit(void) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    if(dill_fast(ctx->poller_init)) dill_pollset_term();
}
#endif

static void dill_resume(struct dill_cr *cr, int id, int err) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    cr->id = id;
    cr->err = err;
    dill_slist_push_back(&ctx->ready, &cr->ready);
}

int dill_canblock(void) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    if(ctx->r->no_blocking1 || ctx->r->no_blocking2) {
        errno = ECANCELED;
        return -1;
    }
    return 0;
}

int dill_no_blocking2(int val) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    int old = ctx->r->no_blocking2;
    ctx->r->no_blocking2 = val;
    return old;
}

/******************************************************************************/
/*  Poller.                                                                   */
/******************************************************************************/

static void dill_poller_init(void) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    /* If intialisation was already done, do nothing. */
    if(dill_fast(ctx->poller_init)) return;
    ctx->poller_init = 1;
    /* Timers. */
    dill_list_init(&ctx->timers);
    /* Polling-mechanism-specific intitialisation. */
    int rc = dill_pollset_init();
    dill_assert(rc == 0);
    /* Register cleanup function once for main thread. */
#if defined(DILL_VALGRIND) || defined(DILL_THREADS)
    rc = dill_atexit(dill_pollset_atexit);
    dill_assert(rc == 0);
#endif
}

/* Adds a timer clause to the list of waited for clauses. */
void dill_timer(struct dill_tmcl *tmcl, int id, int64_t deadline) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    dill_poller_init();
    /* If the deadline is infinite there's nothing to wait for. */
    if(deadline < 0) return;
    /* Finite deadline. */
    tmcl->deadline = deadline;
    /* Move the timer into the right place in the ordered list
       of existing timers. TODO: This is an O(n) operation! */
    struct dill_list_item *it = dill_list_begin(&ctx->timers);
    while(it) {
        struct dill_tmcl *itcl = dill_cont(it, struct dill_tmcl, cl.epitem);
        /* If multiple timers expire at the same momemt they will be fired
           in the order they were created in (> rather than >=). */
        if(itcl->deadline > tmcl->deadline)
            break;
        it = dill_list_next(it);
    }
    dill_waitfor(&tmcl->cl, id, &ctx->timers, it);
}

int dill_in(struct dill_clause *cl, int id, int fd) {
    dill_poller_init();
    if(dill_slow(fd < 0 || fd >= dill_maxfds())) {errno = EBADF; return -1;}
    return dill_pollset_in(cl, id, fd);
}

int dill_out(struct dill_clause *cl, int id, int fd) {
    dill_poller_init();
    if(dill_slow(fd < 0 || fd >= dill_maxfds())) {errno = EBADF; return -1;}
    return dill_pollset_out(cl, id, fd);
}

void dill_clean(int fd) {
    dill_poller_init();
    dill_pollset_clean(fd);
}

/* Wait for external events such as timers or file descriptors. If block is set
   to 0 the function will poll for events and return immediately. If it is set
   to 1 it will block until there's at least one event to process. */
static void dill_poller_wait(int block) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    dill_poller_init();
    while(1) {
        /* Compute timeout for the subsequent poll. */
        int timeout = 0;
        if(block) {
            if(dill_list_empty(&ctx->timers))
                timeout = -1;
            else {
                int64_t nw = now();
                int64_t deadline = dill_cont(dill_list_begin(&ctx->timers),
                    struct dill_tmcl, cl.epitem)->deadline;
                timeout = (int) (nw >= deadline ? 0 : deadline - nw);
            }
        }
        /* Wait for events. */
        int fired = dill_pollset_poll(timeout);
        /* Fire all expired timers. */
        if(!dill_list_empty(&ctx->timers)) {
            int64_t nw = now();
            while(!dill_list_empty(&ctx->timers)) {
                struct dill_tmcl *tmcl = dill_cont(
                    dill_list_begin(&ctx->timers), struct dill_tmcl, cl.epitem);
                if(tmcl->deadline > nw)
                    break;
                dill_list_erase(&ctx->timers, dill_list_begin(&ctx->timers));
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
static void *dill_cr_query(struct hvfs *vfs, const void *type);
static void dill_cr_close(struct hvfs *vfs);

/******************************************************************************/
/*  Creation and termination of coroutines.                                   */
/******************************************************************************/

static void dill_cancel(struct dill_cr *cr, int err);

/* The intial part of go(). Allocates a new stack and handle. */
int dill_prologue(sigjmp_buf **jb, void **ptr, size_t len,
      const char *file, int line) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) {errno = ECANCELED; return -1;}
    struct dill_cr *cr;
    size_t stacksz;
    if(!*ptr) {
        /* Allocate new stack. */
        cr = (struct dill_cr*)dill_allocstack(&stacksz);
        if(dill_slow(!cr)) return -1;
    }
    else {
        /* Stack is supplied by the user.
           Align top of the stack to 16-byte boundary. */
        uintptr_t top = (uintptr_t)*ptr;
        top += len;
        top &= ~(uintptr_t)15;
        stacksz = top - (uintptr_t)*ptr;
        cr = (struct dill_cr*)top;
        if(dill_slow(stacksz < sizeof(struct dill_cr))) {
            errno = ENOMEM; return -1;}
    }
#if defined DILL_CENSUS
    /* Mark the bytes in stack as unused. */
    uint8_t *bottom = ((char*)cr) - stacksz;
    int i;
    for(i = 0; i != stacksz; ++i)
        bottom[i] = 0xa0 + (i % 13);
#endif
    --cr;
    cr->vfs.query = dill_cr_query;
    cr->vfs.close = dill_cr_close;
    int hndl = hmake(&cr->vfs);
    if(dill_slow(hndl < 0)) {
        int err = errno; dill_freestack(cr + 1); errno = err; return -1;}
    dill_slist_item_init(&cr->ready);
    dill_slist_init(&cr->clauses);
    cr->closer = NULL;
    cr->cls = NULL;
    cr->no_blocking1 = 0;
    cr->no_blocking2 = 0;
    cr->done = 0;
    cr->go_stack = *ptr ? 1 : 0;
#if defined DILL_VALGRIND
    cr->sid = VALGRIND_STACK_REGISTER((char*)(cr + 1) - stacksz, cr);
#endif
#if defined DILL_THREADS
    if(dill_slow(!ctx->initialized)) {
        rc = dill_atexit(dill_cr_atexit);
        dill_assert(rc == 0);
        ctx->initialized = 1;
    }
#endif
#if defined DILL_CENSUS
    /* Initialize census. */
    if(dill_slow(!ctx->census_init)) {
        rc = dill_atexit(dill_census_atexit);
        dill_assert(rc == 0);
        ctx->census_init = 1;
    }
    /* Find the appropriate census item if it exists. It's O(n) but meh. */
    struct dill_slist_item *it;
    cr->census = NULL;
    for(it = dill_slist_begin(&ctx->census); it; it = dill_slist_next(it)) {
        cr->census = dill_cont(it, struct dill_census_item, crs);
        if(cr->census->line == line && strcmp(cr->census->file, file) == 0)
            break;
    }
    /* Allocate it if it does not exist. */
    if(!it) {
        cr->census = malloc(sizeof(struct dill_census_item));
        dill_assert(cr->census);
        dill_slist_item_init(&cr->census->crs);
        dill_slist_push(&ctx->census, &cr->census->crs);
        cr->census->file = file;
        cr->census->line = line;
        cr->census->max_stack = 0;
    }
    cr->stacksz = stacksz - sizeof(struct dill_cr);
#endif
    /* Return the context of the parent coroutine to the caller so that it can
       store its current state. It can't be done here becuse we are at the
       wrong stack frame here. */
    *jb = &ctx->r->ctx;
    /* Add parent coroutine to the list of coroutines ready for execution. */
    dill_resume(ctx->r, 0, 0);
    /* Mark the new coroutine as running. */
    *ptr = ctx->r = cr;
    return hndl;
}

/* The final part of go(). Gets called one the coroutine is finished. */
void dill_epilogue(void) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    /* Mark the coroutine as finished. */
    ctx->r->done = 1;
    /* If there's a coroutine waiting till we finish, unblock it now. */
    if(ctx->r->closer)
        dill_cancel(ctx->r->closer, 0);
    /* With no clauses added, this call will never return. */
    dill_assert(dill_slist_empty(&ctx->r->clauses));
    dill_wait();
}

static void *dill_cr_query(struct hvfs *vfs, const void *type) {
    if(dill_slow(type != dill_cr_type)) {errno = ENOTSUP; return NULL;}
    struct dill_cr *cr = dill_cont(vfs, struct dill_cr, vfs);
    return cr;
}

/* Gets called when coroutine handle is closed. */
static void dill_cr_close(struct hvfs *vfs) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    struct dill_cr *cr = dill_cont(vfs, struct dill_cr, vfs);
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
        cr->closer = ctx->r;
        int rc = dill_wait();
        dill_assert(rc == -1 && errno == 0);
    }
#if defined DILL_CENSUS
    /* Find first overwritten byte on the stack.
       Determine stack usage based on that. */
    uint8_t *bottom = ((uint8_t*)cr) - cr->stacksz;
    int i;
    for(i = 0; i != cr->stacksz; ++i) {
        if(bottom[i] != 0xa0 + (i % 13)) {
            /* dill_cr is located on stack so we have take that to account.
               Also, it may be necessary to align the top of the stack to
               16-byte boundary, so add 16 bytes to account for that. */
            size_t used = cr->stacksz - i - sizeof(struct dill_cr) + 16;
            if(used > cr->census->max_stack)
                cr->census->max_stack = used;
            break;
        }
    }
#endif
#if defined DILL_VALGRIND
    VALGRIND_STACK_DEREGISTER(cr->sid);
#endif
    /* Now that the coroutine is finished deallocate it. */
    if(!cr->go_stack) dill_freestack(cr + 1);
}

/******************************************************************************/
/*  Suspend/resume functionality.                                             */
/******************************************************************************/

void dill_waitfor(struct dill_clause *cl, int id,
      struct dill_list *eplist, struct dill_list_item *before) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    /* Add the clause to the endpoint's list of waiting clauses. */
    dill_list_item_init(&cl->epitem);
    dill_list_insert(eplist, &cl->epitem, before);
    cl->eplist = eplist;
    /* Add clause to the coroutine list of active clauses. */
    cl->cr = ctx->r;
    dill_slist_item_init(&cl->item);
    dill_slist_push_back(&ctx->r->clauses, &cl->item);
    cl->id = id;
}

int dill_wait(void)  {
/* Even if process never gets idle, we have to process external events
   once in a while. The external signal may very well be a deadline or
   a user-issued command that cancels the CPU intensive operation. */
    struct dill_ctx_cr *ctx = dill_cr_init();
    /* 103 is a prime. That way it's less likely to coincide with some kind
       of cycle in the user's code. */
    if(ctx->wait_counter >= 103) {
        dill_poller_wait(0);
        ctx->wait_counter = 0;
    }
    /* Store the context of the current coroutine, if any. */
    if(ctx->r) {
        if(dill_setjmp(ctx->r->ctx)) {
            /* We get here once the coroutine is resumed. */
            dill_slist_init(&ctx->r->clauses);
            errno = ctx->r->err;
            return ctx->r->id;
        }
    }
    while(1) {
        /* If there's a coroutine ready to be executed jump to it. */
        if(!dill_slist_empty(&ctx->ready)) {
            ++ctx->wait_counter;
            struct dill_slist_item *it = dill_slist_pop(&ctx->ready);
            ctx->r = dill_cont(it, struct dill_cr, ready);
            dill_longjmp(ctx->r->ctx);
        }
        /* Otherwise, we are going to wait for sleeping coroutines
           and for external events. */
        dill_poller_wait(1);
        /* Sanity check: External events must have unblocked at least
           one coroutine. */
        dill_assert(!dill_slist_empty(&ctx->ready));
        ctx->wait_counter = 0;
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
    struct dill_ctx_cr *ctx = dill_cr_init();
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Put the current coroutine into the ready queue. */
    dill_resume(ctx->r, 0, 0);
    /* Suspend. */
    return dill_wait();
}

/******************************************************************************/
/*  Coroutine-local storage.                                                  */
/******************************************************************************/

void *cls(void) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    return ctx->r->cls;
}

void setcls(void *val) {
    struct dill_ctx_cr *ctx = dill_cr_init();
    ctx->r->cls = val;
}

