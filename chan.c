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

struct dill_chan {
    /* Table of virtual functions. */
    struct hvfs vfs;
    /* The size of one element stored in the channel, in bytes. */
    size_t sz;
    /* List of clauses wanting to receive from the channel. */
    struct dill_list in;
    /* List of clauses wanting to send to the channel. */
    struct dill_list out;
    /* 1 if hdone() has been called on this channel. 0 otherwise. */
    unsigned int done : 1;
    /* 1 if the object was created with chmake_mem(). */
    unsigned int mem : 1;
};

/* Channel clause. */
struct dill_chclause {
    struct dill_clause cl;
    /* An item in either the dill_chan::in or dill_chan::out list. */
    struct dill_list item;
    void *val;
};

DILL_CT_ASSERT(sizeof(struct chmem) >= sizeof(struct dill_chan));

/******************************************************************************/
/*  Handle implementation.                                                    */
/******************************************************************************/

static const int dill_chan_type_placeholder = 0;
static const void *dill_chan_type = &dill_chan_type_placeholder;
static void *dill_chan_query(struct hvfs *vfs, const void *type);
static void dill_chan_close(struct hvfs *vfs);
static int dill_chan_done(struct hvfs *vfs);

/******************************************************************************/
/*  Channel creation and deallocation.                                        */
/******************************************************************************/

int chmake_mem(size_t itemsz, struct chmem *mem) {
    if(dill_slow(!mem)) {errno = EINVAL; return -1;}
    /* Returns ECANCELED if the coroutine is shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    struct dill_chan *ch = (struct dill_chan*)mem;
    ch->vfs.query = dill_chan_query;
    ch->vfs.close = dill_chan_close;
    ch->vfs.done = dill_chan_done;
    ch->sz = itemsz;
    dill_list_init(&ch->in);
    dill_list_init(&ch->out);
    ch->done = 0;
    ch->mem = 1;
    /* Allocate a handle to point to the channel. */
    return hmake(&ch->vfs);
}

int chmake(size_t itemsz) {
    struct dill_chan *ch = malloc(sizeof(struct dill_chan));
    if(dill_slow(!ch)) {errno = ENOMEM; return -1;}
    int h = chmake_mem(itemsz, (struct chmem*)ch);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(ch);
        errno = err;
        return -1;
    }
    ch->mem = 0;
    return h;
}

static void *dill_chan_query(struct hvfs *vfs, const void *type) {
    if(dill_fast(type == dill_chan_type)) return vfs;
    errno = ENOTSUP;
    return NULL;
}

static void dill_chan_close(struct hvfs *vfs) {
    struct dill_chan *ch = (struct dill_chan*)vfs;
    dill_assert(ch);
    /* Resume any remaining senders and receivers on the channel
       with the EPIPE error. */
    while(!dill_list_empty(&ch->in)) {
        struct dill_chclause *chcl = dill_cont(dill_list_next(&ch->in),
            struct dill_chclause, item);
        dill_trigger(&chcl->cl, EPIPE);
    }
    while(!dill_list_empty(&ch->out)) {
        struct dill_chclause *chcl = dill_cont(dill_list_next(&ch->out),
            struct dill_chclause, item);
        dill_trigger(&chcl->cl, EPIPE);
    }
    if(!ch->mem) free(ch);
}

/******************************************************************************/
/*  Sending and receiving.                                                    */
/******************************************************************************/

static void dill_chcancel(struct dill_clause *cl) {
    struct dill_chclause *chcl = dill_cont(cl, struct dill_chclause, cl);
    dill_list_erase(&chcl->item);
}

int chsend(int h, const void *val, size_t len, int64_t deadline) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Get the channel interface. */
    struct dill_chan *ch = hquery(h, dill_chan_type);
    if(dill_slow(!ch)) return -1;
    /* Check that the length provided matches the channel length */
    if(dill_slow(len != ch->sz)) {errno = EINVAL; return -1;}
    /* Check if the channel is done. */
    if(dill_slow(ch->done)) {errno = EPIPE; return -1;}
        /* Copy the message directly to the waiting receiver, if any. */
    if(!dill_list_empty(&ch->in)) {
        struct dill_chclause *chcl = dill_cont(dill_list_next(&ch->in),
            struct dill_chclause, item);
        memcpy(chcl->val, val, len);
        dill_trigger(&chcl->cl, 0);
        return 0;
    }
    /* The clause is not available immediately. */
    if(dill_slow(deadline == 0)) {errno = ETIMEDOUT; return -1;}
    /* Let's wait. */
    struct dill_chclause chcl;
    dill_list_insert(&chcl.item, &ch->out);
    chcl.val = (void*)val;
    dill_waitfor(&chcl.cl, 0, dill_chcancel);
    struct dill_tmclause tmcl;
    dill_timer(&tmcl, 1, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == 1)) {errno = ETIMEDOUT; return -1;}
    if(dill_slow(errno != 0)) return -1;
    return 0;
}

int chrecv(int h, void *val, size_t len, int64_t deadline) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Get the channel interface. */
    struct dill_chan *ch = hquery(h, dill_chan_type);
    if(dill_slow(!ch)) return -1;
    /* Check that the length provided matches the channel length */
    if(dill_slow(len != ch->sz)) {errno = EINVAL; return -1;}
    /* Check whether the channel is done. */
    if(dill_slow(ch->done)) {errno = EPIPE; return -1;}
    /* If there's a sender waiting, copy the message directly from the sender. */
    if(!dill_list_empty(&ch->out)) {
        struct dill_chclause *chcl = dill_cont(dill_list_next(&ch->out),
            struct dill_chclause, item);
        memcpy(val, chcl->val, len);
        dill_trigger(&chcl->cl, 0);
        return 0;
    }
    /* The clause is not immediately available. */
    if(dill_slow(deadline == 0)) {errno = ETIMEDOUT; return -1;}
    /* Let's wait. */
    struct dill_chclause chcl;
    dill_list_insert(&chcl.item, &ch->in);
    chcl.val = val;
    dill_waitfor(&chcl.cl, 0, dill_chcancel);
    struct dill_tmclause tmcl;
    dill_timer(&tmcl, 1, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == 1)) {errno = ETIMEDOUT; return -1;}
    if(dill_slow(errno != 0)) return -1;
    return 0;
}

static int dill_chan_done(struct hvfs *vfs) {
    struct dill_chan *ch = (struct dill_chan*)vfs;
    dill_assert(ch);
    if(ch->done) {errno = EPIPE; return -1;}
    ch->done = 1;
    /* Resume any remaining senders and receivers on the channel
       with the EPIPE error. */
    while(!dill_list_empty(&ch->in)) {
        struct dill_chclause *chcl = dill_cont(dill_list_next(&ch->in),
            struct dill_chclause, item);
        dill_trigger(&chcl->cl, EPIPE);
    }
    while(!dill_list_empty(&ch->out)) {
        struct dill_chclause *chcl = dill_cont(dill_list_next(&ch->out),
            struct dill_chclause, item);
        dill_trigger(&chcl->cl, EPIPE);
    }
    return 0;
}

int choose(struct chclause *clauses, int nclauses, int64_t deadline) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    if(dill_slow(nclauses < 0 || (nclauses != 0 && !clauses))) {
        errno = EINVAL; return -1;}
    int i;
    for(i = 0; i != nclauses; ++i) {
        struct chclause *cl = &clauses[i];
        struct dill_chan *ch = hquery(cl->ch, dill_chan_type);
        if(dill_slow(!ch)) return i;
        if(dill_slow(cl->len != ch->sz || (cl->len > 0 && !cl->val))) {
            errno = EINVAL; return i;}
        struct dill_chclause *chcl;
        switch(cl->op) {
        case CHSEND:
            if(dill_slow(ch->done)) {errno = EPIPE; return i;}
            if(dill_list_empty(&ch->in)) break;
            chcl = dill_cont(dill_list_next(&ch->in),
                struct dill_chclause, item);
            memcpy(chcl->val, cl->val, cl->len);
            dill_trigger(&chcl->cl, 0);
            errno = 0;
            return i;
        case CHRECV:
            if(dill_slow(ch->done)) {errno = EPIPE; return i;}
            if(dill_list_empty(&ch->out)) break;
            chcl = dill_cont(dill_list_next(&ch->out),
                struct dill_chclause, item);
            memcpy(cl->val, chcl->val, ch->sz);
            dill_trigger(&chcl->cl, 0);
            errno = 0;
            return i;
        default:
            errno = EINVAL;
            return i;
        } 
    }
    /* There are no clauses immediately available. */
    if(dill_slow(deadline == 0)) {errno = ETIMEDOUT; return -1;}
    /* Let's wait. */
    struct dill_chclause chcls[nclauses];
    for(i = 0; i != nclauses; ++i) {
        struct dill_chan *ch = hquery(clauses[i].ch, dill_chan_type);
        dill_assert(ch);
        dill_list_insert(&chcls[i].item,
            clauses[i].op == CHRECV ? &ch->in : &ch->out);
        chcls[i].val = clauses[i].val;
        dill_waitfor(&chcls[i].cl, i, dill_chcancel);
    }
    struct dill_tmclause tmcl;
    dill_timer(&tmcl, nclauses, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == nclauses)) {errno = ETIMEDOUT; return -1;}
    return id;
}

