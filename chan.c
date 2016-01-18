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

chan dill_chmake(size_t itemsz, size_t bufsz, const char *created) {
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
    dill_trace(created, "<%d>=chmake(%d)", (int)ch->debug.id, (int)bufsz);
    return ch;
}

chan dill_chdup(chan ch, const char *current) {
    if(dill_slow(!ch))
        dill_panic("null channel used");
    dill_trace(current, "chdup(<%d>)", (int)ch->debug.id);
    ++ch->refcount;
    return ch;
}

void dill_chclose(chan ch, const char *current) {
    if(dill_slow(!ch))
        dill_panic("null channel used");
    dill_trace(current, "chclose(<%d>)", (int)ch->debug.id);
    assert(ch->refcount > 0);
    --ch->refcount;
    if(ch->refcount)
        return;
    if(!dill_list_empty(&ch->sender.clauses) ||
          !dill_list_empty(&ch->receiver.clauses))
        dill_panic("attempt to close a channel while it is still being used");
    dill_unregister_chan(&ch->debug);
    free(ch);
}

/* Unblock a coroutine blocked in dill_choose_wait() function.
   It also cleans up the associated clause list. */
static void dill_choose_unblock(struct dill_clause *cl) {
    struct dill_slist_item *it;
    struct dill_clause *itcl;
    for(it = dill_slist_begin(&cl->cr->choosedata.clauses);
          it; it = dill_slist_next(it)) {
        itcl = dill_cont(it, struct dill_clause, chitem);
        if(!itcl->used)
            continue;
        dill_list_erase(&itcl->ep->clauses, &itcl->epitem);
    }
    if(cl->cr->choosedata.ddline > 0)
        dill_timer_rm(&cl->cr->timer);
    dill_resume(cl->cr, cl->idx);
}

static void dill_choose_callback(struct dill_timer *timer) {
    struct dill_cr *cr = dill_cont(timer, struct dill_cr, timer);
    struct dill_slist_item *it;
    for(it = dill_slist_begin(&cr->choosedata.clauses);
          it; it = dill_slist_next(it)) {
        struct dill_clause *itcl = dill_cont(it, struct dill_clause, chitem);
        dill_assert(itcl->used);
        dill_list_erase(&itcl->ep->clauses, &itcl->epitem);
    }
    dill_resume(cr, -1);
}

/* Push new item to the channel. */
static void dill_enqueue(chan ch, void *val) {
    /* If there's a receiver already waiting, let's resume it. */
    if(!dill_list_empty(&ch->receiver.clauses)) {
        dill_assert(ch->items == 0);
        struct dill_clause *cl = dill_cont(
            dill_list_begin(&ch->receiver.clauses), struct dill_clause, epitem);
        memcpy(cl->val, val, ch->sz);
        dill_choose_unblock(cl);
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
        dill_choose_unblock(cl);
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
        dill_choose_unblock(cl);
    }
}

static int dill_choose_(struct chclause *clauses, int nclauses,
      int64_t deadline) {
    if(dill_slow(nclauses < 0 || (nclauses && !clauses))) {
        errno = EINVAL;
        return -1;
    }

    /* Create unique ID for each invocation of choose(). It is used to identify
       and ignore duplicate entries in the pollset. */
    static uint64_t seq = 0;
    ++seq;

    dill_slist_init(&dill_running->choosedata.clauses);
    dill_running->choosedata.ddline = -1;

    int available = 0;
    int i;
    for(i = 0; i != nclauses; ++i) {
        struct dill_clause *cl = (struct dill_clause*)&clauses[i];
        if(dill_slow(!cl->channel || cl->channel->sz != cl->len)) {
            errno = EINVAL;
            return -1;
        }
        switch(cl->op) {
        case CHOOSE_CHS:
            /* Cannot send to done-with channel. */
            if(dill_slow(cl->channel->done)) {
                errno = EPIPE;
                return -1;
            }
            cl->available = !dill_list_empty(&cl->channel->receiver.clauses) ||
                cl->channel->items < cl->channel->bufsz ? 1 : 0;
            cl->ep = &cl->channel->sender;
            break;
        case CHOOSE_CHR:
            cl->available = cl->channel->done ||
                !dill_list_empty(&cl->channel->sender.clauses) ||
                cl->channel->items ? 1 : 0;
            cl->ep = &cl->channel->receiver;
            break;
        default:
            errno = EINVAL;
            return -1;
        }
        cl->cr = dill_running;
        cl->idx = i;
        cl->used = 1;
        if(cl->ep->seq == seq)
            continue;
        cl->ep->seq = seq;
        dill_slist_push_back(&dill_running->choosedata.clauses, &cl->chitem);
        if(cl->available)
            ++available;
    }

    struct dill_choosedata *cd = &dill_running->choosedata;
    struct dill_slist_item *it;
    struct dill_clause *cl;

    /* If there are clauses that are immediately available
       randomly choose one of them. */
    if(available > 0) {
        int chosen = available == 1 ? 0 : (int)(random() % available);
        for(it = dill_slist_begin(&cd->clauses); it; it = dill_slist_next(it)) {
            cl = dill_cont(it, struct dill_clause, chitem);
            if(!cl->available)
                continue;
            if(!chosen)
                break;
            --chosen;
        }
        if(cl->op == CHOOSE_CHS)
            dill_enqueue(cl->channel, cl->val);
        else
            dill_dequeue(cl->channel, cl->val);
        dill_resume(dill_running, cl->idx);
        return dill_suspend(NULL);
    }

    /* If immediate execution was requested, exit now. */
    if(deadline == 0) {
        dill_resume(dill_running, -1);
        dill_suspend(NULL);
        errno = ETIMEDOUT;
        return -1;
    }

    /* If deadline was specified, start the timer. */
    if(deadline > 0) {
        cd->ddline = deadline;
        dill_timer_add(&dill_running->timer, deadline, dill_choose_callback);
    }

    /* In all other cases register this coroutine with the queried channels
       and wait till one of the clauses unblocks. */
    for(it = dill_slist_begin(&cd->clauses); it; it = dill_slist_next(it)) {
        cl = dill_cont(it, struct dill_clause, chitem);
        dill_list_insert(&cl->ep->clauses, &cl->epitem, NULL);
    }
    /* If there are multiple parallel chooses done from different coroutines
       all but one must be blocked on the following line. */
    int res = dill_suspend(NULL);
    if(res == -1)
        errno = ETIMEDOUT;
    return res;
}

int dill_choose(struct chclause *clauses, int nclauses, int64_t deadline,
      const char *current) {
    dill_trace(current, "choose()");
    dill_running->state = DILL_CHOOSE;
    dill_running->nclauses = nclauses;
    dill_running->clauses = clauses;
    dill_set_current(&dill_running->debug, current);
    return dill_choose_(clauses, nclauses, deadline);
}

int dill_chs(chan ch, const void *val, size_t len, const char *current) {
    if(dill_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    dill_trace(current, "chs(<%d>)", (int)ch->debug.id);
    dill_running->state = DILL_CHS;
    struct chclause cl = {ch, CHOOSE_CHS, (void*)val, len};
    return dill_choose_(&cl, 1, -1);
}

int dill_chr(chan ch, void *val, size_t len, const char *current) {
    if(dill_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    dill_trace(current, "chr(<%d>)", (int)ch->debug.id);
    dill_running->state = DILL_CHR;
    struct chclause cl = {ch, CHOOSE_CHR, val, len};
    return dill_choose_(&cl, 1, -1);
}

int dill_chdone(chan ch, const void *val, size_t len, const char *current) {
    if(dill_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    dill_trace(current, "chdone(<%d>)", (int)ch->debug.id);
    if(dill_slow(ch->done))
        dill_panic("chdone on already done-with channel");
    /* Panic if there are other senders on the same channel. */
    if(dill_slow(!dill_list_empty(&ch->sender.clauses)))
        dill_panic("send to done-with channel");
    /* Put the channel into done-with mode. */
    ch->done = 1;
    /* Store the terminal value into a special position in the channel. */
    memcpy(((char*)(ch + 1)) + (ch->bufsz * ch->sz) , val, ch->sz);
    /* Resume all the receivers currently waiting on the channel. */
    while(!dill_list_empty(&ch->receiver.clauses)) {
        struct dill_clause *cl = dill_cont(
            dill_list_begin(&ch->receiver.clauses), struct dill_clause, epitem);
        memcpy(cl->val, val, ch->sz);
        dill_choose_unblock(cl);
    }
    return 0;
}

