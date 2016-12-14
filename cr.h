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

#ifndef DILL_CR_INCLUDED
#define DILL_CR_INCLUDED

#include <stdint.h>

#include "libdill.h"
#include "list.h"
#include "slist.h"

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
    /* There are two possible reasons to disable blocking calls.
       1. The coroutine is being closed by its owner.
       2. The execution is happening within a context of hclose() function. */
    unsigned int no_blocking1 : 1;
    unsigned int no_blocking2 : 1;
    /* Set once coroutine has finished its execution. */
    unsigned int done : 1;
    /* If true, the coroutine was launched via go_mem. */
    unsigned int mem : 1;
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

struct dill_ctx_cr {
    /* Currently running coroutine. */
    struct dill_cr *r;
    /* List of coroutines ready for execution. */
    struct dill_slist ready;
    /* Global linked list of all timers. The list is ordered.
       First timer to be resumed comes first and so on. */
    struct dill_list timers;
    int wait_counter;
    /* Main coroutine. We don't control creation of main coroutine's stack
       so we have to store this info here instead on the top of the stack. */
    struct dill_cr main;
#if defined DILL_CENSUS
    struct dill_slist census;
#endif
};

struct dill_clause {
    /* The coroutine that owns this clause. */
    struct dill_cr *cr;
    /* List of clauses coroutine is waiting for. See dill_cr::clauses. */
    struct dill_slist_item item;
    /* These two fields are completely opaque to the coroutine. They are meant
       to be used by endpoints. The only thing coroutine does with it is, just
       before dill_wait() exits, it removes 'epitem' from 'eplist' for each
       clause. 'eplist' can be NULL in which case removal doesn't happen. */
    struct dill_list_item epitem;
    struct dill_list *eplist;
    /* Number to return from dill_wait() if this clause triggers. */
    int id;
};

/* Timer clause. */
struct dill_tmcl {
    struct dill_clause cl;
    int64_t deadline;
};

int dill_ctx_cr_init(struct dill_ctx_cr *ctx);
void dill_ctx_cr_term(struct dill_ctx_cr *ctx);

/* When dill_wait() is called next time, the coroutine will wait
   (among other clauses) for this clause. 'id' must not be negative.
   'eplist' is the list to add the clause to (can be NULL). 'epitem' is the
   item in the list to insert the clause before. If NULL it will be inserted
   at the end of the list. Call to dill_wait() will remove the clause from
   the list. */
void dill_waitfor(struct dill_clause *cl, int id,
    struct dill_list *eplist, struct dill_list_item *before);

/* Suspend running coroutine. Move to executing different coroutines.
   The coroutine will be resumed once one of the clauses previously added by
   dill_waitfor() is triggered. Once that happens all the clauses, whether
   triggered or not, will be canceled. Function returns ID of the triggered
   clause or -1 in case of error. In either case it sets errno to 0 indicate
   success or non-zero value to indicate error. */
int dill_wait(void);

/* Schedule previously suspended coroutine for execution. Keep in mind that it
   doesn't immediately run it, just put it into the queue of ready coroutines.
   It will cause dill_wait() return the id supplied in dill_waitfor(). */
void dill_trigger(struct dill_clause *cl, int err);

/* Add timer to the list of active clauses. */
void dill_timer(struct dill_tmcl *tmcl, int id, int64_t deadline);

/* Wait for in event on a file descriptor. */
int dill_in(struct dill_clause *cl, int id, int fd);

/* Wait for out event on a file descriptor. */
int dill_out(struct dill_clause *cl, int id, int fd);

/* Returns 0 if blocking functions are allowed.
   Returns -1 and sets errno to ECANCELED otherwise. */
int dill_canblock(void);

/* TODO: Can we get rid of this function? */
int dill_no_blocking2(int val);

/* Cleans cached info about the fd. */
void dill_clean(int fd);

#endif


