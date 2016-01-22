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

DILL_CT_ASSERT(sizeof(struct dill_choosedata) <= DILL_OPAQUE_SIZE);

chan dill_channel(size_t itemsz, size_t bufsz, const char *created) {
    /* If there's at least one channel created in the user's code
       we want the debug functions to get into the binary. */
    dill_preserve_debug();
    /* We are allocating 1 additional element after the channel buffer to
       store the done-with value. It can't be stored in the regular buffer
       because that would mean chdone() would block when buffer is full. */
    struct dill_chan *ch = (struct dill_chan*)
        malloc(sizeof(struct dill_chan) + (itemsz * (bufsz + 1)));
    if(!ch)
        return NULL;
    dill_register_chan(&ch->debug, created);
    ch->sz = itemsz;
    ch->sender.seq = 0;
    dill_list_init(&ch->sender.clauses);
    ch->receiver.seq = 0;
    dill_list_init(&ch->receiver.clauses);
    ch->refcount = 1;
    ch->done = 0;
    ch->bufsz = bufsz;
    ch->items = 0;
    ch->first = 0;
    dill_trace(created, "<%d>=channel(%d, %d)", (int)ch->debug.id,
        (int)itemsz, (int)bufsz);
    return ch;
}

chan dill_chdup(chan ch, const char *current) {
    if(dill_slow(!ch))
       return NULL;
    dill_trace(current, "chdup(<%d>)", (int)ch->debug.id);
    ++ch->refcount;
    return ch;
}

void dill_chclose(chan ch, const char *current) {
    if(dill_slow(!ch))
        return;
    dill_trace(current, "chclose(<%d>)", (int)ch->debug.id);
    assert(ch->refcount > 0);
    --ch->refcount;
    if(ch->refcount)
        return;
    /* Resume any remaining senders and receivers on the channel
       with EPIPE error. */
    struct dill_list_item *it = dill_list_begin(&ch->sender.clauses);
    while(it) {
        struct dill_list_item *next = dill_list_next(it);
        struct dill_clause *cl = dill_cont(it, struct dill_clause, epitem);
        dill_resume(cl->cr, -EPIPE);
        it = next;
    }
    it = dill_list_begin(&ch->receiver.clauses);
    while(it) {
        struct dill_list_item *next = dill_list_next(it);
        struct dill_clause *cl = dill_cont(it, struct dill_clause, epitem);
        dill_resume(cl->cr, -EPIPE);
        it = next;
    }
    dill_unregister_chan(&ch->debug);
    free(ch);
}

static struct dill_ep *dill_getep(struct dill_clause *cl) {
    return cl->op == CHSEND ?
        &cl->channel->sender : &cl->channel->receiver;
}

static void dill_choose_unblock_cb(struct dill_cr *cr) {
    struct dill_choosedata *cd = (struct dill_choosedata*)cr->opaque;
    int i;
    for(i = 0; i != cd->nclauses; ++i) {
        struct dill_clause *cl = &cd->clauses[i];
        dill_list_erase(&dill_getep(cl)->clauses, &cl->epitem);
    }
    if(cd->ddline > 0)
        dill_timer_rm(&cr->timer);
}

/* Returns index of the clause in the pollset. */
static int dill_choose_index(struct dill_clause *cl) {
    struct dill_choosedata *cd = (struct dill_choosedata*)cl->cr->opaque;
    return cl - cd->clauses;
}

/* Push new item to the channel. */
static void dill_enqueue(chan ch, void *val) {
    /* If there's a receiver already waiting, let's resume it. */
    if(!dill_list_empty(&ch->receiver.clauses)) {
        dill_assert(ch->items == 0);
        struct dill_clause *cl = dill_cont(
            dill_list_begin(&ch->receiver.clauses), struct dill_clause, epitem);
        memcpy(cl->val, val, ch->sz);
        dill_resume(cl->cr, dill_choose_index(cl));
        return;
    }
    /* Write the value to the buffer. */
    assert(ch->items < ch->bufsz);
    size_t pos = (ch->first + ch->items) % ch->bufsz;
    memcpy(((char*)(ch + 1)) + (pos * ch->sz) , val, ch->sz);
    ++ch->items;
}

/* Pop one value from the channel. */
static void dill_dequeue(chan ch, void *val) {
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
        memcpy(val, cl->val, ch->sz);
        dill_resume(cl->cr, dill_choose_index(cl));
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
        memcpy(((char*)(ch + 1)) + (pos * ch->sz) , cl->val, ch->sz);
        ++ch->items;
        dill_resume(cl->cr, dill_choose_index(cl));
    }
}

static int dill_choose_available(struct dill_clause *cl) {
    if(cl->op == CHSEND) {
        return !dill_list_empty(&cl->channel->receiver.clauses) ||
        cl->channel->items < cl->channel->bufsz ? 1 : 0;
    }
    else {
        return cl->channel->done ||
            !dill_list_empty(&cl->channel->sender.clauses) ||
            cl->channel->items ? 1 : 0;
    }
}

static int dill_choose_(struct chclause *clauses, int nclauses,
      int64_t deadline) {
    if(dill_slow(nclauses < 0 || (nclauses && !clauses))) {
        errno = EINVAL;
        return -1;
    }
    if(dill_slow(dill_running->canceler)) {
        errno = ECANCELED;
        return -1;
    }

    /* Create unique ID for each invocation of choose(). It is used to
       identify and ignore duplicate entries in the pollset. */
    static uint64_t seq = 0;
    ++seq;

    /* Initialise the operation. */
    struct dill_clause *cls = (struct dill_clause*)clauses;
    struct dill_choosedata *cd = (struct dill_choosedata*)dill_running->opaque;
    cd->nclauses = nclauses;
    cd->clauses = cls;
    cd->ddline = -1;

    /* Find out which clauses are immediately available. */
    int available = 0;
    int i;
    for(i = 0; i != nclauses; ++i) {
        if(dill_slow(!cls[i].channel || cls[i].channel->sz != cls[i].len ||
              (cls[i].op != CHSEND && cls[i].op != CHRECV))) {
            errno = EINVAL;
            return -1;
        }
        if(dill_slow(cls[i].op == CHSEND && cls[i].channel->done)) {
            errno = EPIPE;
            return -1;
        }
        cls[i].cr = dill_running;
        struct dill_ep *ep = dill_getep(&cls[i]);
        if(ep->seq == seq)
            continue;
        ep->seq = seq;
        if(dill_choose_available(&cls[i])) {
            cls[available].aidx = i;
            ++available;
        }
    }

    /* If there are clauses that are immediately available
       randomly choose one of them. */
    if(available > 0) {
        int chosen = available == 1 ? 0 : (int)(random() % available);
        struct dill_clause *cl = &cls[cls[chosen].aidx];
        if(cl->op == CHSEND)
            dill_enqueue(cl->channel, cl->val);
        else
            dill_dequeue(cl->channel, cl->val);
        dill_resume(dill_running, dill_choose_index(cl));
        return dill_suspend(NULL);
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
        cd->ddline = deadline;
        dill_timer_add(&dill_running->timer, deadline);
    }

    /* In all other cases register this coroutine with the queried channels
       and wait till one of the clauses unblocks. */
    for(i = 0; i != cd->nclauses; ++i) {
        struct dill_clause *cl = &cd->clauses[i];
        dill_list_insert(&dill_getep(cl)->clauses, &cl->epitem, NULL);
    }
    /* If there are multiple parallel chooses done from different coroutines
       all but one must be blocked on the following line. */
    int res = dill_suspend(dill_choose_unblock_cb);
    if(dill_slow(res < 0)) {
        errno = -res;
        return -1;
    }
    return res;
}

int dill_choose(struct chclause *clauses, int nclauses, int64_t deadline,
      const char *current) {
    dill_trace(current, "choose()");
    dill_startop(&dill_running->debug, DILL_CHOOSE, current);
    return dill_choose_(clauses, nclauses, deadline);
}

int dill_chsend(chan ch, const void *val, size_t len, const char *current) {
    if(dill_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    dill_trace(current, "chsend(<%d>)", (int)ch->debug.id);
    dill_startop(&dill_running->debug, DILL_CHSEND, current);
    struct chclause cl = {ch, CHSEND, (void*)val, len};
    return dill_choose_(&cl, 1, -1);
}

int dill_chrecv(chan ch, void *val, size_t len, const char *current) {
    if(dill_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    dill_trace(current, "chrecv(<%d>)", (int)ch->debug.id);
    dill_startop(&dill_running->debug, DILL_CHRECV, current);
    struct chclause cl = {ch, CHRECV, val, len};
    return dill_choose_(&cl, 1, -1);
}

int dill_chdone(chan ch, const void *val, size_t len, const char *current) {
    if(dill_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    dill_trace(current, "chdone(<%d>)", (int)ch->debug.id);
    if(dill_slow(ch->done)) {
        errno = EPIPE;
        return -1;
    }
    /* Resume any remaining senders on the channel with EPIPE error. */
    struct dill_list_item *it = dill_list_begin(&ch->sender.clauses);
    while(it) {
        struct dill_list_item *next = dill_list_next(it);
        struct dill_clause *cl = dill_cont(it, struct dill_clause, epitem);
        dill_resume(cl->cr, -EPIPE);
        it = next;
    }
    /* Put the channel into done-with mode. */
    ch->done = 1;
    /* Store the terminal value into a special position in the channel. */
    memcpy(((char*)(ch + 1)) + (ch->bufsz * ch->sz) , val, ch->sz);
    /* Resume all the receivers currently waiting on the channel. */
    while(!dill_list_empty(&ch->receiver.clauses)) {
        struct dill_clause *cl = dill_cont(
            dill_list_begin(&ch->receiver.clauses), struct dill_clause, epitem);
        memcpy(cl->val, val, ch->sz);
        dill_resume(cl->cr, dill_choose_index(cl));
    }
    return 0;
}

