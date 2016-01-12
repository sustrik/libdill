/*

  Copyright (c) 2015 Martin Sustrik

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
#include "treestack.h"
#include "utils.h"

static int ts_choose_seqnum = 0;

struct ts_chan *ts_getchan(struct ts_ep *ep) {
    switch(ep->type) {
    case TS_SENDER:
        return ts_cont(ep, struct ts_chan, sender);
    case TS_RECEIVER:
        return ts_cont(ep, struct ts_chan, receiver);
    default:
        assert(0);
    }
}

chan ts_chmake(size_t itemsz, size_t bufsz, const char *created) {
    /* If there's at least one channel created in the user's code
       we want the debug functions to get into the binary. */
    ts_preserve_debug();
    /* We are allocating 1 additional element after the channel buffer to
       store the done-with value. It can't be stored in the regular buffer
       because that would mean chdone() would block when buffer is full. */
    struct ts_chan *ch = (struct ts_chan*)
        malloc(sizeof(struct ts_chan) + (itemsz * (bufsz + 1)));
    if(!ch)
        return NULL;
    ts_register_chan(&ch->debug, created);
    ch->sz = itemsz;
    ch->sender.type = TS_SENDER;
    ch->sender.seqnum = ts_choose_seqnum;
    ts_list_init(&ch->sender.clauses);
    ch->receiver.type = TS_RECEIVER;
    ch->receiver.seqnum = ts_choose_seqnum;
    ts_list_init(&ch->receiver.clauses);
    ch->refcount = 1;
    ch->done = 0;
    ch->bufsz = bufsz;
    ch->items = 0;
    ch->first = 0;
    ts_trace(created, "<%d>=chmake(%d)", (int)ch->debug.id, (int)bufsz);
    return ch;
}

chan ts_chdup(chan ch, const char *current) {
    if(ts_slow(!ch))
        ts_panic("null channel used");
    ts_trace(current, "chdup(<%d>)", (int)ch->debug.id);
    ++ch->refcount;
    return ch;
}

void ts_chclose(chan ch, const char *current) {
    if(ts_slow(!ch))
        ts_panic("null channel used");
    ts_trace(current, "chclose(<%d>)", (int)ch->debug.id);
    assert(ch->refcount > 0);
    --ch->refcount;
    if(ch->refcount)
        return;
    if(!ts_list_empty(&ch->sender.clauses) ||
          !ts_list_empty(&ch->receiver.clauses))
        ts_panic("attempt to close a channel while it is still being used");
    ts_unregister_chan(&ch->debug);
    free(ch);
}

/* Unblock a coroutine blocked in ts_choose_wait() function.
   It also cleans up the associated clause list. */
static void ts_choose_unblock(struct ts_clause *cl) {
    struct ts_slist_item *it;
    struct ts_clause *itcl;
    for(it = ts_slist_begin(&cl->cr->choosedata.clauses);
          it; it = ts_slist_next(it)) {
        itcl = ts_cont(it, struct ts_clause, chitem);
        if(!itcl->used)
            continue;
        ts_list_erase(&itcl->ep->clauses, &itcl->epitem);
    }
    if(cl->cr->choosedata.ddline > 0)
        ts_timer_rm(&cl->cr->timer);
    ts_resume(cl->cr, cl->idx);
}

static void ts_choose_callback(struct ts_timer *timer) {
    struct ts_cr *cr = ts_cont(timer, struct ts_cr, timer);
    struct ts_slist_item *it;
    for(it = ts_slist_begin(&cr->choosedata.clauses);
          it; it = ts_slist_next(it)) {
        struct ts_clause *itcl = ts_cont(it, struct ts_clause, chitem);
        ts_assert(itcl->used);
        ts_list_erase(&itcl->ep->clauses, &itcl->epitem);
    }
    ts_resume(cr, -1);
}

/* Push new item to the channel. */
static void ts_enqueue(chan ch, void *val) {
    /* If there's a receiver already waiting, let's resume it. */
    if(!ts_list_empty(&ch->receiver.clauses)) {
        ts_assert(ch->items == 0);
        struct ts_clause *cl = ts_cont(
            ts_list_begin(&ch->receiver.clauses), struct ts_clause, epitem);
        memcpy(cl->val, val, ch->sz);
        ts_choose_unblock(cl);
        return;
    }
    /* Write the value to the buffer. */
    assert(ch->items < ch->bufsz);
    size_t pos = (ch->first + ch->items) % ch->bufsz;
    memcpy(((char*)(ch + 1)) + (pos * ch->sz) , val, ch->sz);
    ++ch->items;
}

/* Pop one value from the channel. */
static void ts_dequeue(chan ch, void *val) {
    /* Get a blocked sender, if any. */
    struct ts_clause *cl = ts_cont(
        ts_list_begin(&ch->sender.clauses), struct ts_clause, epitem);
    if(!ch->items) {
        /* If chdone was already called we can return the value immediately.
           There are no senders waiting to send. */
        if(ts_slow(ch->done)) {
            ts_assert(!cl);
            memcpy(val, ((char*)(ch + 1)) + (ch->bufsz * ch->sz), ch->sz);
            return;
        }
        /* Otherwise there must be a sender waiting to send. */
        ts_assert(cl);
        memcpy(val, cl->val, ch->sz);
        ts_choose_unblock(cl);
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
        ts_choose_unblock(cl);
    }
}

static int ts_choose_(struct chclause *clauses, int nclauses,
      int64_t deadline) {
    if(ts_slow(nclauses < 0 || (nclauses && !clauses))) {
        errno = EINVAL;
        return -1;
    }

    ts_slist_init(&ts_running->choosedata.clauses);
    ts_running->choosedata.ddline = -1;
    ts_running->choosedata.available = 0;
    ++ts_choose_seqnum;

    int i;
    for(i = 0; i != nclauses; ++i) {
        struct ts_clause *cl = (struct ts_clause*)&clauses[i];
        if(ts_slow(!cl->channel || cl->channel->sz != cl->len)) {
            errno = EINVAL;
            return -1;
        }
        int available;
        switch(cl->op) {
        case CHOOSE_CHS:
            /* Cannot send to done-with channel. */
            if(ts_slow(cl->channel->done)) {
                errno = EPIPE;
                return -1;
            }
            /* Find out whether the clause is immediately available. */
            available = !ts_list_empty(&cl->channel->receiver.clauses) ||
                cl->channel->items < cl->channel->bufsz ? 1 : 0;
            if(available)
                ++ts_running->choosedata.available;
            /* Fill in the reserved fields in clause entry. */
            cl->cr = ts_running;
            cl->ep = &cl->channel->sender;
            cl->available = available;
            cl->idx = i;
            cl->used = 1;
            ts_slist_push_back(&ts_running->choosedata.clauses, &cl->chitem);
            break;
        case CHOOSE_CHR:
            /* Find out whether the clause is immediately available. */
            available = cl->channel->done ||
                !ts_list_empty(&cl->channel->sender.clauses) ||
                cl->channel->items ? 1 : 0;
            if(available)
                ++ts_running->choosedata.available;
            /* Fill in the clause entry. */
            cl->cr = ts_running;
            cl->ep = &cl->channel->receiver;
            cl->idx = i;
            cl->available = available;
            cl->used = 1;
            ts_slist_push_back(&ts_running->choosedata.clauses, &cl->chitem);
            break;
        default:
            errno = EINVAL;
            return -1;
        }
        if(cl->ep->seqnum == ts_choose_seqnum) {
            ++cl->ep->refs;
        }
        else {
            cl->ep->seqnum = ts_choose_seqnum;
            cl->ep->refs = 1;
            cl->ep->tmp = -1;
        }
    }

    struct ts_choosedata *cd = &ts_running->choosedata;
    struct ts_slist_item *it;
    struct ts_clause *cl;

    /* If there are clauses that are immediately available
       randomly choose one of them. */
    if(cd->available > 0) {
        int chosen = cd->available == 1 ? 0 : (int)(random() % (cd->available));
        for(it = ts_slist_begin(&cd->clauses); it; it = ts_slist_next(it)) {
            cl = ts_cont(it, struct ts_clause, chitem);
            if(!cl->available)
                continue;
            if(!chosen)
                break;
            --chosen;
        }
        struct ts_chan *ch = ts_getchan(cl->ep);
        if(cl->ep->type == TS_SENDER)
            ts_enqueue(ch, cl->val);
        else
            ts_dequeue(ch, cl->val);
        ts_resume(ts_running, cl->idx);
        return ts_suspend();
    }

    /* If immediate execution was requested, exit now. */
    if(deadline == 0) {
        ts_resume(ts_running, -1);
        ts_suspend();
        errno = ETIMEDOUT;
        return -1;
    }

    /* If deadline was specified, start the timer. */
    if(deadline > 0) {
        cd->ddline = deadline;
        ts_timer_add(&ts_running->timer, deadline, ts_choose_callback);
    }

    /* In all other cases register this coroutine with the queried channels
       and wait till one of the clauses unblocks. */
    for(it = ts_slist_begin(&cd->clauses); it; it = ts_slist_next(it)) {
        cl = ts_cont(it, struct ts_clause, chitem);
        if(ts_slow(cl->ep->refs > 1)) {
            if(cl->ep->tmp == -1)
                cl->ep->tmp =
                    cl->ep->refs == 1 ? 0 : (int)(random() % cl->ep->refs);
            if(cl->ep->tmp) {
                --cl->ep->tmp;
                cl->used = 0;
                continue;
            }
            cl->ep->tmp = -2;
        }
        ts_list_insert(&cl->ep->clauses, &cl->epitem, NULL);
    }
    /* If there are multiple parallel chooses done from different coroutines
       all but one must be blocked on the following line. */
    int res = ts_suspend();
    if(res == -1)
        errno = ETIMEDOUT;
    return res;
}

int ts_choose(struct chclause *clauses, int nclauses, int64_t deadline,
      const char *current) {
    ts_trace(current, "choose()");
    ts_running->state = TS_CHOOSE;
    ts_running->nclauses = nclauses;
    ts_running->clauses = clauses;
    ts_set_current(&ts_running->debug, current);
    return ts_choose_(clauses, nclauses, deadline);
}

int ts_chs(chan ch, const void *val, size_t len, const char *current) {
    if(ts_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    ts_trace(current, "chs(<%d>)", (int)ch->debug.id);
    ts_running->state = TS_CHS;
    struct chclause cl = {ch, CHOOSE_CHS, (void*)val, len};
    return ts_choose_(&cl, 1, -1);
}

int ts_chr(chan ch, void *val, size_t len, const char *current) {
    if(ts_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    ts_trace(current, "chr(<%d>)", (int)ch->debug.id);
    ts_running->state = TS_CHR;
    struct chclause cl = {ch, CHOOSE_CHR, val, len};
    return ts_choose_(&cl, 1, -1);
}

int ts_chdone(chan ch, const void *val, size_t len, const char *current) {
    if(ts_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    ts_trace(current, "chdone(<%d>)", (int)ch->debug.id);
    if(ts_slow(ch->done))
        ts_panic("chdone on already done-with channel");
    /* Panic if there are other senders on the same channel. */
    if(ts_slow(!ts_list_empty(&ch->sender.clauses)))
        ts_panic("send to done-with channel");
    /* Put the channel into done-with mode. */
    ch->done = 1;
    /* Store the terminal value into a special position in the channel. */
    memcpy(((char*)(ch + 1)) + (ch->bufsz * ch->sz) , val, ch->sz);
    /* Resume all the receivers currently waiting on the channel. */
    while(!ts_list_empty(&ch->receiver.clauses)) {
        struct ts_clause *cl = ts_cont(
            ts_list_begin(&ch->receiver.clauses), struct ts_clause, epitem);
        memcpy(cl->val, val, ch->sz);
        ts_choose_unblock(cl);
    }
    return 0;
}

