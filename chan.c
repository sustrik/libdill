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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cr.h"
#include "libdill.h"
#include "list.h"
#include "utils.h"

/* The channel. Memory layout of a channel looks like this:
   +-----------+-------+--------+---------------------------------+--------+
   | dill_chan | item1 | item 2 |             ...                 | item N |
   +-----------+-------+--------+---------------------------------+--------+
*/
struct dill_chan {
    /* The size of one element stored in the channel, in bytes. */
    size_t sz;
    /* List of clauses wanting to receive from the channel. */
    struct dill_list in;
    /* List of clauses wanting to send to the channel. */
    struct dill_list out;
    /* 1 is chdone() was already called. 0 otherwise. */
    int done;
    /* The message buffer directly follows the chan structure. 'bufsz' specifies
       the maximum capacity of the buffer. 'items' is the number of messages
       currently in the buffer. 'first' is the index of the next message to
       be received from the buffer. There's one extra element at the end of
       the buffer used to store the message supplied by chdone() function. */
    size_t bufsz;
    size_t items;
    size_t first;
};

/* Channel clause. */
struct dill_chcl {
    struct dill_clause cl;
    void *val;
};

/* Make sure that the channel clause will fit into the opaque data space
   int chclause strcuture. */
DILL_CT_ASSERT(sizeof(struct dill_chcl) <= 64);

/******************************************************************************/
/*  Handle implementation.                                                    */
/******************************************************************************/

static const int dill_chan_type_placeholder = 0;
static const void *dill_chan_type = &dill_chan_type_placeholder;
static void dill_chan_close(int h);
static const struct hvfptrs dill_chan_vfptrs = {dill_chan_close};

/******************************************************************************/
/*  Channel creation and deallocation.                                        */
/******************************************************************************/

int channel(size_t itemsz, size_t bufsz) {
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Allocate the channel structure followed by the item buffer. */
    struct dill_chan *ch = (struct dill_chan*)
        malloc(sizeof(struct dill_chan) + (itemsz * bufsz));
    if(!ch) {errno = ENOMEM; return -1;}
    ch->sz = itemsz;
    dill_list_init(&ch->in);
    dill_list_init(&ch->out);
    ch->done = 0;
    ch->bufsz = bufsz;
    ch->items = 0;
    ch->first = 0;
    /* Allocate a handle to point to the channel. */
    int h = handle(dill_chan_type, ch, &dill_chan_vfptrs);
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
    while(!dill_list_empty(&ch->in)) {
        struct dill_clause *cl = dill_cont(dill_list_begin(&ch->in),
            struct dill_clause, epitem);
        dill_trigger(cl, EPIPE);
    }
    while(!dill_list_empty(&ch->out)) {
        struct dill_clause *cl = dill_cont(dill_list_begin(&ch->out),
            struct dill_clause, epitem);
        dill_trigger(cl, EPIPE);
    }
    free(ch);
}

/******************************************************************************/
/*  Sending and receiving.                                                    */
/******************************************************************************/

int chsend(int h, const void *val, size_t len, int64_t deadline) {
    struct chclause cl = {CHSEND, h, (void*)val, len};
    int rc = choose(&cl, 1, deadline);
    if(dill_slow(rc < 0 || errno != 0)) return -1;
    return 0;
}

int chrecv(int h, void *val, size_t len, int64_t deadline) {
    struct chclause cl = {CHRECV, h, val, len};
    int rc = choose(&cl, 1, deadline);
    if(dill_slow(rc < 0 || errno != 0)) return -1;
    return 0;
}

int chdone(int h) {
    struct dill_chan *ch = hdata(h, dill_chan_type);
    if(dill_slow(!ch)) return -1;
    ch->done = 1;
    /* Resume any remaining senders and receivers on the channel
       with EPIPE error. */
    while(!dill_list_empty(&ch->in)) {
        struct dill_clause *cl = dill_cont(dill_list_begin(&ch->in),
            struct dill_clause, epitem);
        dill_trigger(cl, EPIPE);
    }
    while(!dill_list_empty(&ch->out)) {
        struct dill_clause *cl = dill_cont(dill_list_begin(&ch->out),
            struct dill_clause, epitem);
        dill_trigger(cl, EPIPE);
    }
    return 0;
}

int choose(struct chclause *clauses, int nclauses, int64_t deadline) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    if(dill_slow(nclauses < 0 || (nclauses != 0 && !clauses))) {
        errno = EINVAL; return -1;}
    /* First pass through the clauses. Find out which are immediately ready. */
    int available = 0;
    int i;
    for(i = 0; i != nclauses; ++i) {
        struct dill_chan *ch = hdata(clauses[i].ch, dill_chan_type);
        if(dill_slow(!ch)) return i;
        if(dill_slow(clauses[i].len != ch->sz ||
              (clauses[i].len > 0 && !clauses[i].val))) {
            errno = EINVAL; return i;}
        switch(clauses[i].op) {
        case CHSEND:
            if(ch->items < ch->bufsz || !dill_list_empty(&ch->in) || ch->done) {
                *(int*)&clauses[available].reserved = i;
                available++;
            }
            break;
        case CHRECV:
            if(ch->items || !dill_list_empty(&ch->out) || ch->done) {
                *(int*)&clauses[available].reserved = i;
                available++;
            }
            break;
        default:
            errno = EINVAL;
            return i;
        } 
    }
    /* If there's at least one clause available immediately, choose one of
       them at random and execute it. */
    if(available) {
        int chosen = *(int*)&clauses[random() % available].reserved;
        struct chclause *cl = &clauses[chosen];
        struct dill_chan *ch = hdata(cl->ch, dill_chan_type);
        dill_assert(ch);
        if(dill_slow(ch->done)) {errno = EPIPE; return chosen;}
        switch(cl->op) {
        case CHSEND:
            if(!dill_list_empty(&ch->in)) {
                /* Copy the message directly to the waiting receiver. */
                struct dill_chcl *chcl = dill_cont(dill_list_begin(&ch->in),
                    struct dill_chcl, cl.epitem);
                memcpy(chcl->val, cl->val, cl->len);
                dill_trigger(&chcl->cl, 0);
                errno = 0;
                return chosen;
            }
            dill_assert(ch->items < ch->bufsz);
            /* Write the item to the buffer. */
            size_t pos = (ch->first + ch->items) % ch->bufsz;
            memcpy(((char*)(ch + 1)) + (pos * ch->sz) , cl->val, cl->len);
            ++ch->items;
            errno = 0;
            return chosen;
        case CHRECV:
            if(!ch->items) {
                /* Unbuffered channel. But there's a sender waiting to send.
                   Copy the message directly from a waiting sender. */
                struct dill_chcl *chcl = dill_cont(dill_list_begin(&ch->out),
                    struct dill_chcl, cl.epitem);
                memcpy(cl->val, chcl->val, ch->sz);
                dill_trigger(&chcl->cl, 0);
                errno = 0;
                return chosen;
            }
            /* Read an item from the buffer. */
            memcpy(cl->val, ((char*)(ch + 1)) + (ch->first * ch->sz), ch->sz);
            ch->first = (ch->first + 1) % ch->bufsz;
            --ch->items;
            /* If there was a waiting sender, unblock it. */
            if(!dill_list_empty(&ch->out)) {
                struct dill_chcl *chcl = dill_cont(dill_list_begin(&ch->out),
                    struct dill_chcl, cl.epitem);
                size_t pos = (ch->first + ch->items) % ch->bufsz;
                memcpy(((char*)(ch + 1)) + (pos * ch->sz) , chcl->val, ch->sz);
                ++ch->items;
                dill_trigger(&chcl->cl, 0);
            }
            errno = 0;
            return chosen;
        default:
            dill_assert(0);
        }
    }
    /* There are no clauses available immediately. */
    if(dill_slow(deadline == 0)) {errno = ETIMEDOUT; return -1;}
    /* Let's wait. */
    for(i = 0; i != nclauses; ++i) {
        struct dill_chan *ch = hdata(clauses[i].ch, dill_chan_type);
        struct dill_chcl *chcl = (struct dill_chcl*)&clauses[i].reserved;
        chcl->val = clauses[i].val;
        dill_waitfor(&chcl->cl, i,
            clauses[i].op == CHRECV ? &ch->in : &ch->out, NULL);
    }
    struct dill_tmcl tmcl;
    if(deadline > 0)
        dill_timer(&tmcl, nclauses, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == nclauses)) {errno = ETIMEDOUT; return -1;}
    return id;
}

