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

#ifndef DILL_CR_INCLUDED
#define DILL_CR_INCLUDED

#include <stdint.h>

#include "list.h"
#include "qlist.h"
#include "rbtree.h"
#include "slist.h"

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"

/* The coroutine. The memory layout looks like this:
   +-------------------------------------------------------------+---------+
   |                                                      stack  | dill_cr |
   +-------------------------------------------------------------+---------+
   - dill_cr contains generic book-keeping info about the coroutine
   - the stack is a standard C stack; it grows downwards (at the moment, libdill
     doesn't support microarchitectures where stacks grow upwards)
*/
struct dill_cr {
    /* When the coroutine is ready for execution but not running yet,
       it lives on this list (ctx->ready). 'id' is the result value to return
       from dill_wait() when the coroutine is resumed. Additionally, errno
       will be set to 'err'. */
    struct dill_slist ready;
    /* Virtual function table. */
    struct dill_hvfs vfs;
    int id;
    int err;
    /* When the coroutine is suspended 'ctx' holds the context
       (registers and such).*/
    sigjmp_buf ctx;
    /* If the coroutine is blocked, here's the list of the clauses it's
       waiting for. */
    struct dill_slist clauses;
    /* A list of coroutines belonging to a particular bundle. */
    struct dill_list bundle;
    /* There are two possible reasons to disable blocking calls.
       1. The coroutine is being closed by its owner.
       2. The execution is happening within the context of an hclose() call. */
    unsigned int no_blocking1 : 1;
    unsigned int no_blocking2 : 1;
    /* Set when the coroutine has finished its execution. */
    unsigned int done : 1;
    /* If true, the coroutine was launched with go_mem. */
    unsigned int mem : 1;
    /* When the coroutine handle is being closed, this points to the
       coroutine that is doing the hclose() call. */
    struct dill_cr *closer;
#if defined DILL_VALGRIND
    /* Valgrind stack identifier. This way, valgrind knows which areas of
       memory are used as stacks, and so it doesn't produce spurious warnings.
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
   architectures. To achieve this, we align this structure (with the added
   benefit of a minor optimization). */
} __attribute__((aligned(16)));

struct dill_ctx_cr {
    /* Currently running coroutine. */
    struct dill_cr *r;
    /* List of coroutines ready for execution. */
    struct dill_qlist ready;
    /* All active timers. */
    struct dill_rbtree timers;
    /* Last time poll was performed. */
    int64_t last_poll;
    /* The main coroutine. We don't control the creation of the main coroutine's
       stack, so we have to store this info here instead of the top of
       the stack. */
    struct dill_cr main;
#if defined DILL_CENSUS
    struct dill_slist census;
#endif
};

struct dill_clause {
    /* The coroutine that owns this clause. */
    struct dill_cr *cr;
    /* List of the clauses the coroutine is waiting on. See dill_cr::clauses. */
    struct dill_slist item;
    /* Number to return from dill_wait() if this clause triggers. */
    int id;
    /* Function to call when this clause is canceled. */
    void (*cancel)(struct dill_clause *cl);
};

/* Timer clause. */
struct dill_tmclause {
    struct dill_clause cl;
    /* An item in dill_ctx_cr::timers. */
    struct dill_rbtree_item item;
};

/* File descriptor clause. */
struct dill_fdclause;

int dill_ctx_cr_init(struct dill_ctx_cr *ctx);
void dill_ctx_cr_term(struct dill_ctx_cr *ctx);

/* When dill_wait() is called next time, the coroutine will wait
   (among other clauses) on this clause. 'id' must not be negative.
   'cancel' is a function to be called when the clause is canceled
   without being triggered. */
void dill_waitfor(struct dill_clause *cl, int id,
    void (*cancel)(struct dill_clause *cl));

/* Suspend running coroutine. Move to executing different coroutines.
   The coroutine will be resumed once one of the clauses previously added by
   dill_waitfor() is triggered. When that happens, all the clauses, whether
   triggered or not, will be canceled. The function returns the ID of the
   triggered clause or -1 on error. In either case, it sets errno to 0 indicate
   success or non-zero value to indicate error. */
int dill_wait(void);

/* Schedule a previously suspended coroutine for execution. Keep in mind that
   this doesn't immediately run it, it just puts it into the coroutine ready
   queue. It will cause dill_wait() to return the id supplied in
   dill_waitfor(). */
void dill_trigger(struct dill_clause *cl, int err);

/* Add a timer to the list of active clauses. */
void dill_timer(struct dill_tmclause *tmcl, int id, int64_t deadline);

/* Returns 0 if blocking functions are allowed.
   Returns -1 and sets errno to ECANCELED otherwise. */
int dill_canblock(void);

/* When set to 1, blocking calls return ECANCELED.
   Returns the old value of the flag */
int dill_no_blocking(int val);

/* Cleans cached info about the fd. */
int dill_clean(int fd);

#endif


