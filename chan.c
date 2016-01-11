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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chan.h"
#include "cr.h"
#include "debug.h"
#include "treestack.h"
#include "utils.h"

TS_CT_ASSERT(TS_CLAUSELEN == sizeof(struct ts_clause));

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

chan ts_chmake(size_t sz, size_t bufsz, const char *created) {
    /* If there's at least one channel created in the user's code
       we want the debug functions to get into the binary. */
    ts_preserve_debug();
    /* We are allocating 1 additional element after the channel buffer to
       store the done-with value. It can't be stored in the regular buffer
       because that would mean chdone() would block when buffer is full. */
    struct ts_chan *ch = (struct ts_chan*)
        malloc(sizeof(struct ts_chan) + (sz * (bufsz + 1)));
    if(!ch)
        return NULL;
    ts_register_chan(&ch->debug, created);
    ch->sz = sz;
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
    if(cl->cr->choosedata.ddline >= 0)
        ts_timer_rm(&cl->cr->timer);
    ts_resume(cl->cr, cl->idx);
}

static void ts_choose_init_(const char *current) {
    ts_set_current(&ts_running->debug, current);
    ts_slist_init(&ts_running->choosedata.clauses);
    ts_running->choosedata.othws = 0;
    ts_running->choosedata.ddline = -1;
    ts_running->choosedata.available = 0;
    ++ts_choose_seqnum;
}

void ts_choose_init(const char *current) {
    ts_trace(current, "choose()");
    ts_running->state = TS_CHOOSE;
    ts_choose_init_(current);
}

void ts_choose_in(void *clause, chan ch, size_t sz, int idx) {
    if(ts_slow(!ch))
        ts_panic("null channel used");
    if(ts_slow(ch->sz != sz))
        ts_panic("receive of a type not matching the channel");
    /* Find out whether the clause is immediately available. */
    int available = ch->done || !ts_list_empty(&ch->sender.clauses) ||
        ch->items ? 1 : 0;
    if(available)
        ++ts_running->choosedata.available;
    /* If there are available clauses don't bother with non-available ones. */
    if(!available && ts_running->choosedata.available)
        return;
    /* Fill in the clause entry. */
    struct ts_clause *cl = (struct ts_clause*) clause;
    cl->cr = ts_running;
    cl->ep = &ch->receiver;
    cl->val = NULL;
    cl->idx = idx;
    cl->available = available;
    cl->used = 1;
    ts_slist_push_back(&ts_running->choosedata.clauses, &cl->chitem);
    if(cl->ep->seqnum == ts_choose_seqnum) {
        ++cl->ep->refs;
        return;
    }
    cl->ep->seqnum = ts_choose_seqnum;
    cl->ep->refs = 1;
    cl->ep->tmp = -1;
}

void ts_choose_out(void *clause, chan ch, const void *val, size_t sz, int idx) {
    if(ts_slow(!ch))
        ts_panic("null channel used");
    if(ts_slow(ch->done))
        ts_panic("send to done-with channel");
    if(ts_slow(ch->sz != sz))
        ts_panic("send of a type not matching the channel");
    /* Find out whether the clause is immediately available. */
    int available = !ts_list_empty(&ch->receiver.clauses) ||
        ch->items < ch->bufsz ? 1 : 0;
    if(available)
        ++ts_running->choosedata.available;
    /* If there are available clauses don't bother with non-available ones. */
    if(!available && ts_running->choosedata.available)
        return;
    /* Fill in the clause entry. */
    struct ts_clause *cl = (struct ts_clause*) clause;
    cl->cr = ts_running;
    cl->ep = &ch->sender;
    cl->val = (void*)val;
    cl->available = available;
    cl->idx = idx;
    cl->used = 1;
    ts_slist_push_back(&ts_running->choosedata.clauses, &cl->chitem);
    if(cl->ep->seqnum == ts_choose_seqnum) {
        ++cl->ep->refs;
        return;
    }
    cl->ep->seqnum = ts_choose_seqnum;
    cl->ep->refs = 1;
    cl->ep->tmp = -1;
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

void ts_choose_deadline(int64_t ddline) {
    if(ts_slow(ts_running->choosedata.othws ||
          ts_running->choosedata.ddline >= 0))
        ts_panic(
            "multiple 'otherwise' or 'deadline' clauses in a choose statement");
    /* Infinite deadline clause can never fire so we can as well ignore it. */
    if(ddline < 0)
        return;
    ts_running->choosedata.ddline = ddline;
}

void ts_choose_otherwise(void) {
    if(ts_slow(ts_running->choosedata.othws ||
          ts_running->choosedata.ddline >= 0))
        ts_panic(
            "multiple 'otherwise' or 'deadline' clauses in a choose statement");
    ts_running->choosedata.othws = 1;
}

/* Push new item to the channel. */
static void ts_enqueue(chan ch, void *val) {
    /* If there's a receiver already waiting, let's resume it. */
    if(!ts_list_empty(&ch->receiver.clauses)) {
        ts_assert(ch->items == 0);
        struct ts_clause *cl = ts_cont(
            ts_list_begin(&ch->receiver.clauses), struct ts_clause, epitem);
        memcpy(ts_valbuf(cl->cr, ch->sz), val, ch->sz);
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

int ts_choose_wait(void) {
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
            ts_dequeue(ch, ts_valbuf(cl->cr, ch->sz));
        ts_resume(ts_running, cl->idx);
        return ts_suspend();
    }

    /* If not so but there's an 'otherwise' clause we can go straight to it. */
    if(cd->othws) {
        ts_resume(ts_running, -1);
        return ts_suspend();
    }

    /* If deadline was specified, start the timer. */
    if(cd->ddline >= 0)
        ts_timer_add(&ts_running->timer, cd->ddline, ts_choose_callback);

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
    return ts_suspend();
}

void *ts_choose_val(size_t sz) {
    /* The assumption here is that by supplying the same size as before
       we are going to get the same buffer which already has the data
       written into it. */
    return ts_valbuf(ts_running, sz);
}

int ts_chs(chan ch, const void *val, size_t len, const char *current) {
    if(ts_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    ts_trace(current, "chs(<%d>)", (int)ch->debug.id);
    ts_choose_init_(current);
    ts_running->state = TS_CHS;
    struct ts_clause cl;
    ts_choose_out(&cl, ch, val, len, 0);
    ts_choose_wait();
    return 0;
}

int ts_chr(chan ch, void *val, size_t len, const char *current) {
    if(ts_slow(!ch || !val || len != ch->sz)) {
        errno = EINVAL;
        return -1;
    }
    ts_trace(current, "chr(<%d>)", (int)ch->debug.id);
    ts_running->state = TS_CHR;
    ts_choose_init_(current);
    struct ts_clause cl;
    ts_choose_in(&cl, ch, len, 0);
    ts_choose_wait();
    void *res = ts_choose_val(len);
    memcpy(val, res, len);
    return 0;
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
        memcpy(ts_valbuf(cl->cr, ch->sz), val, ch->sz);
        ts_choose_unblock(cl);
    }
    return 0;
}

