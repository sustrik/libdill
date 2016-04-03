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

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chan.h"
#include "cr.h"
#include "debug.h"
#include "libdill.h"
#include "utils.h"

static const int dill_chan_type_placeholder = 0;
static const void *dill_chan_type = &dill_chan_type_placeholder;

static void dill_chan_close(int h);
static void dill_chan_dump(int h);

static const struct hvfptrs dill_chan_vfptrs = {
    dill_chan_close,
    dill_chan_dump
};

int dill_channel(size_t itemsz, size_t bufsz, const char *created) {
    /* If there's at least one channel created in the user's code
       we want the debug functions to get into the binary. */
    dill_preserve_debug();
    /* Allocate the channel structure followed by the item buffer. */
    struct dill_chan *ch = (struct dill_chan*)
        malloc(sizeof(struct dill_chan) + (itemsz * bufsz));
    if(!ch) {errno = ENOMEM; return -1;}
    ch->sz = itemsz;
    ch->sender.seq = 0;
    dill_list_init(&ch->sender.clauses);
    ch->receiver.seq = 0;
    dill_list_init(&ch->receiver.clauses);
    ch->done = 0;
    ch->bufsz = bufsz;
    ch->items = 0;
    ch->first = 0;
    /* Allocate a handle to point to the channel. */
    int h = dill_handle(dill_chan_type, ch, &dill_chan_vfptrs, created);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(ch);
        errno = err;
        return -1;
    }
    return h;
}

static void dill_chan_close(int h) {
    struct dill_chan *ch = hdata(h, dill_chan_type);
    dill_assert(ch);
    /* Resume any remaining senders and receivers on the channel
       with EPIPE error. */
    while(!dill_list_empty(&ch->sender.clauses)) {
        struct dill_clause *cl = dill_cont(dill_list_begin(
            &ch->sender.clauses), struct dill_clause, epitem);
        cl->error = EPIPE;
        dill_resume(cl->cr, cl->idx);
    }
    while(!dill_list_empty(&ch->receiver.clauses)) {
        struct dill_clause *cl = dill_cont(
            dill_list_begin(&ch->receiver.clauses), struct dill_clause, epitem);
        cl->error = EPIPE;
        dill_resume(cl->cr, cl->idx);
    }
    free(ch);
}

static void dill_chan_dump(int h) {
    struct dill_chan *ch = hdata(h, dill_chan_type);
    dill_assert(ch);
    fprintf(stderr, "  CHANNEL item-size:%zu items:%zu/%zu done:%d\n",
        ch->sz, ch->items, ch->bufsz, ch->done);
}

static struct dill_ep *dill_getep(struct dill_clause *cl) {
    struct dill_chan *ch = hdata(cl->i, dill_chan_type);
    return cl->op == CHSEND ? &ch->sender : &ch->receiver;
}

static void dill_choose_unblock_cb(struct dill_cr *cr) {
    int i;
    for(i = 0; i != cr->nclauses; ++i) {
        struct dill_clause *cl = &cr->clauses[i];
        dill_list_erase(&dill_getep(cl)->clauses, &cl->epitem);
    }
    if(cr->ddline > 0)
        dill_timer_rm(&cr->timer);
}

/* Push new item to the channel. */
static void dill_enqueue(struct dill_chan *ch, void *val) {
    /* If there's a receiver already waiting, let's resume it. */
    if(!dill_list_empty(&ch->receiver.clauses)) {
        dill_assert(ch->items == 0);
        struct dill_clause *cl = dill_cont(
            dill_list_begin(&ch->receiver.clauses), struct dill_clause, epitem);
        memcpy(cl->p, val, ch->sz);
        cl->error = 0;
        dill_resume(cl->cr, cl->idx);
        return;
    }
    /* Write the value to the buffer. */
    assert(ch->items < ch->bufsz);
    size_t pos = (ch->first + ch->items) % ch->bufsz;
    memcpy(((char*)(ch + 1)) + (pos * ch->sz) , val, ch->sz);
    ++ch->items;
}

/* Pop one value from the channel. */
static void dill_dequeue(struct dill_chan *ch, void *val) {
    /* Get a blocked sender, if any. */
    struct dill_clause *cl = dill_cont(
        dill_list_begin(&ch->sender.clauses), struct dill_clause, epitem);
    if(!ch->items) {
        /* If chdone was already called we can return the value immediately.
           There are no senders waiting to send. */
        if(dill_slow(ch->done)) {
            dill_assert(!cl);
            memcpy(val, ((char*)(ch + 1)) + (ch->bufsz * ch->sz), ch->sz);
            return;
        }
        /* Otherwise there must be a sender waiting to send. */
        dill_assert(cl);
        memcpy(val, cl->p, ch->sz);
        cl->error = 0;
        dill_resume(cl->cr, cl->idx);
        return;
    }
    /* If there's a value in the buffer start by retrieving it. */
    memcpy(val, ((char*)(ch + 1)) + (ch->first * ch->sz), ch->sz);
    ch->first = (ch->first + 1) % ch->bufsz;
    --ch->items;
    /* And if there was a sender waiting, unblock it. */
    if(cl) {
        assert(ch->items < ch->bufsz);
        size_t pos = (ch->first + ch->items) % ch->bufsz;
        memcpy(((char*)(ch + 1)) + (pos * ch->sz) , cl->p, ch->sz);
        ++ch->items;
        cl->error = 0;
        dill_resume(cl->cr, cl->idx);
    }
}

/* Returns 0 if operation can be performed.
   EAGAIN if it would block.
   EPIPE if it would return EPIPE. */
static int dill_choose_error(struct dill_clause *cl) {
    struct dill_chan *ch = hdata(cl->i, dill_chan_type);
    switch(cl->op) {
    case CHSEND:
        if(ch->done)
            return EPIPE;
        if(dill_list_empty(&ch->receiver.clauses) &&
              ch->items == ch->bufsz)
            return EAGAIN;
        return 0;
    case CHRECV:
        if(!dill_list_empty(&ch->sender.clauses) || ch->items > 0)
            return 0;
        if(ch->done)
            return EPIPE;
        return EAGAIN;
    default:
        dill_assert(0);
    }
}

static int dill_choose_(struct chclause *clauses, int nclauses,
      int64_t deadline) {
    if(dill_slow(nclauses < 0 || (nclauses && !clauses))) {
        errno = EINVAL; return -1;}
    if(dill_slow(dill_cr_isstopped())) {
        errno = ECANCELED; return -1;}
    /* Create unique ID for each invocation of choose(). It is used to
       identify and ignore duplicate entries in the pollset. */
    static uint64_t seq = 0;
    ++seq;
    /* Initialise the operation. */
    struct dill_clause *cls = (struct dill_clause*)clauses;
    dill_running->nclauses = nclauses;
    dill_running->clauses = cls;
    dill_running->ddline = -1;
    /* Find out which clauses are immediately available. */
    int available = 0;
    int chosen = -1;
    int i;
    for(i = 0; i != nclauses; ++i) {
        struct dill_chan *ch = hdata(cls[i].i, dill_chan_type);
        if(dill_slow(!ch)) return -1;
        if(dill_slow(ch->sz != cls[i].sz ||
              (cls[i].sz > 0 && !cls[i].p) ||
              (cls[i].op != CHSEND && cls[i].op != CHRECV))) {
            errno = EINVAL;
            return -1;
        }
        cls[i].idx = i;
        cls[i].cr = dill_running;
        struct dill_ep *ep = dill_getep(&cls[i]);
        if(ep->seq == seq)
            continue;
        ep->seq = seq;
        cls[i].error = dill_choose_error(&cls[i]);
        if(cls[i].error != EAGAIN) {
            ++available;
            if(chosen < 0 || random() % available == 0)
                chosen = i;
        }
    }
    /* If there are clauses that are immediately available
       randomly choose one of them. */
    int res;
    if(available > 0) {
        struct dill_clause *cl = &cls[chosen];
        struct dill_chan *ch = hdata(cl->i, dill_chan_type);
        if(cl->error == 0) {
            if(cl->op == CHSEND)
                dill_enqueue(ch, cl->p);
            else
                dill_dequeue(ch, cl->p);
        }
        dill_resume(dill_running, cl->idx);
        res = dill_suspend(NULL);
        goto finish;
    }
    /* If non-blocking behaviour was requested, exit now. */
    if(deadline == 0) {
        dill_resume(dill_running, -1);
        dill_suspend(NULL);
        errno = ETIMEDOUT;
        return -1;
    }
    /* If deadline was specified, start the timer. */
    if(deadline > 0) {
        dill_running->ddline = deadline;
        dill_timer_add(&dill_running->timer, deadline);
    }
    /* In all other cases register this coroutine with the queried channels
       and wait till one of the clauses unblocks. */
    for(i = 0; i != dill_running->nclauses; ++i) {
        struct dill_clause *cl = &dill_running->clauses[i];
        dill_list_insert(&dill_getep(cl)->clauses, &cl->epitem, NULL);
    }
    /* If there are multiple parallel chooses done from different coroutines
       all but one must be blocked on the following line. */
    res = dill_suspend(dill_choose_unblock_cb);
finish:
    /* Global error, not related to any particular clause. */
    if(dill_slow(res < 0)) {errno = -res; return -1;}
    /* Success or error for the triggered clause. */
    errno = cls[res].error;
    dill_assert(errno == 0 || errno == EPIPE);
    return res;
}

int dill_choose(struct chclause *clauses, int nclauses, int64_t deadline,
      const char *current) {
    return dill_choose_(clauses, nclauses, deadline);
}

int dill_chsend(int ch, const void *val, size_t len, int64_t deadline,
      const char *current) {
    struct chclause cl = {CHSEND, ch, (void*)val, len};
    int res = dill_choose_(&cl, 1, deadline);
    if(dill_slow(res == 0 && errno != 0))
        res = -1;
    return res;
}

int dill_chrecv(int ch, void *val, size_t len, int64_t deadline,
      const char *current) {
    struct chclause cl = {CHRECV, ch, val, len};
    int res = dill_choose_(&cl, 1, deadline);
    if(dill_slow(res == 0 && errno != 0))
        res = -1;
    return res;
}

int dill_chdone(int h, const char *current) {
    struct dill_chan *ch = hdata(h, dill_chan_type);
    if(dill_slow(!ch)) return -1;
    if(dill_slow(ch->done)) {errno = EPIPE; return -1;}
    /* Put the channel into done-with mode. */
    ch->done = 1;
    /* Resume any remaining senders on the channel. */
    while(!dill_list_empty(&ch->sender.clauses)) {
        struct dill_clause *cl = dill_cont(dill_list_begin(&ch->sender.clauses),
            struct dill_clause, epitem);
        cl->error = EPIPE;
        dill_resume(cl->cr, cl->idx);
    }
    /* Resume all the receivers currently waiting on the channel. */
    while(!dill_list_empty(&ch->receiver.clauses)) {
        struct dill_clause *cl = dill_cont(
            dill_list_begin(&ch->receiver.clauses), struct dill_clause, epitem);
        cl->error = EPIPE;
        dill_resume(cl->cr, cl->idx);
    }
    return 0;
}

