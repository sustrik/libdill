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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cr.h"
#include "ctx.h"
#include "list.h"
#include "utils.h"

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"

struct dill_halfchan {
    /* Table of virtual functions. */
    struct dill_hvfs vfs;
    /* List of clauses wanting to receive from the inbound halfchannel. */
    struct dill_list in;
    /* List of clauses wanting to send to the inbound halfchannel. */
    struct dill_list out;
    /* Whether this is the fist or the second half-channel of the channel. */
    unsigned int index : 1;
    /* 1 if chdone() has been called on this channel. 0 otherwise. */
    unsigned int done : 1;
    /* 1 if the object was created with chmake_mem(). */
    unsigned int mem : 1;
    /* 1 if hclose() was already called for this half-channel. */
    unsigned int closed : 1;
};

/* Channel clause. */
struct dill_chanclause {
    struct dill_clause cl;
    /* An item in either the dill_halfchan::in or dill_halfchan::out list. */
    struct dill_list item;
    /* The object being passed via the channel. */
    void *val;
    size_t len;
};

DILL_CT_ASSERT(sizeof(struct dill_chstorage) >=
    sizeof(struct dill_halfchan) * 2);

/******************************************************************************/
/*  Handle implementation.                                                    */
/******************************************************************************/

static const int dill_halfchan_type_placeholder = 0;
const void *dill_halfchan_type = &dill_halfchan_type_placeholder;
static void *dill_halfchan_query(struct dill_hvfs *vfs, const void *type);
static void dill_halfchan_close(struct dill_hvfs *vfs);

/******************************************************************************/
/*  Helpers.                                                                  */
/******************************************************************************/

/* Return the other half-channel within the same channel. */
#define dill_halfchan_other(self) (self->index ? self - 1  : self + 1)

/******************************************************************************/
/*  Channel creation and deallocation.                                        */
/******************************************************************************/

static void dill_halfchan_init(struct dill_halfchan *ch, int index) {
    ch->vfs.query = dill_halfchan_query;
    ch->vfs.close = dill_halfchan_close;
    dill_list_init(&ch->in);
    dill_list_init(&ch->out);
    ch->index = index;
    ch->done = 0;
    ch->mem = 1;
    ch->closed = 0;
}

int dill_chmake_mem(struct dill_chstorage *mem, int chv[2]) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    struct dill_halfchan *ch = (struct dill_halfchan*)mem;
    dill_halfchan_init(&ch[0], 0);
    dill_halfchan_init(&ch[1], 1);
    chv[0] = dill_hmake(&ch[0].vfs);
    if(dill_slow(chv[0] < 0)) {err = errno; goto error1;}
    chv[1] = dill_hmake(&ch[1].vfs);
    if(dill_slow(chv[1] < 0)) {err = errno; goto error2;}
    return 0;
error2:
    /* This closes the handle but leaves everything else alone given
       that the second handle wasn't event created. */
    dill_hclose(chv[0]);
error1:
    errno = err;
    return -1;
}

int dill_chmake(int chv[2]) {
    int err;
    struct dill_chstorage *ch = malloc(sizeof(struct dill_chstorage));
    if(dill_slow(!ch)) {err = ENOMEM; goto error1;}
    int h = dill_chmake_mem(ch, chv);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    ((struct dill_halfchan*)ch)[0].mem = 0;
    ((struct dill_halfchan*)ch)[1].mem = 0;
    return h;
error2:
    free(ch);
error1:
    errno = err;
    return -1;
}

static void *dill_halfchan_query(struct dill_hvfs *vfs, const void *type) {
    if(dill_fast(type == dill_halfchan_type)) return vfs;
    errno = ENOTSUP;
    return NULL;
}

static void dill_halfchan_term(struct dill_halfchan *ch) {
    /* Resume any remaining senders and receivers on the channel
       with the EPIPE error. */
    while(!dill_list_empty(&ch->in)) {
        struct dill_chanclause *chcl = dill_cont(dill_list_next(&ch->in),
            struct dill_chanclause, item);
        dill_trigger(&chcl->cl, EPIPE);
    }
    while(!dill_list_empty(&ch->out)) {
        struct dill_chanclause *chcl = dill_cont(dill_list_next(&ch->out),
            struct dill_chanclause, item);
        dill_trigger(&chcl->cl, EPIPE);
    }
}

static void dill_halfchan_close(struct dill_hvfs *vfs) {
    struct dill_halfchan *ch = (struct dill_halfchan*)vfs;
    dill_assert(ch && !ch->closed);
    /* If the other half of the channel is still open do nothing. */
    if(!dill_halfchan_other(ch)->closed) {
        ch->closed = 1;
        return;
    }
    if(ch->index) ch = dill_halfchan_other(ch);
    dill_halfchan_term(&ch[0]);
    dill_halfchan_term(&ch[1]);
    if(!ch->mem) free(ch);
}

/******************************************************************************/
/*  Sending and receiving.                                                    */
/******************************************************************************/

static void dill_chcancel(struct dill_clause *cl) {
    struct dill_chanclause *chcl = dill_cont(cl, struct dill_chanclause, cl);
    dill_list_erase(&chcl->item);
}

int dill_chsend(int h, const void *val, size_t len, int64_t deadline) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Get the channel interface. */
    struct dill_halfchan *ch = dill_hquery(h, dill_halfchan_type);
    if(dill_slow(!ch)) return -1;
    /* Sending is always done to the opposite side of the channel. */
    ch = dill_halfchan_other(ch);   
    /* Check if the channel is done. */
    if(dill_slow(ch->done)) {errno = EPIPE; return -1;}
    /* Copy the message directly to the waiting receiver, if any. */
    if(!dill_list_empty(&ch->in)) {
        struct dill_chanclause *chcl = dill_cont(dill_list_next(&ch->in),
            struct dill_chanclause, item);
        if(dill_slow(len != chcl->len)) {
            dill_trigger(&chcl->cl, EMSGSIZE);
            errno = EMSGSIZE;
            return -1;
        }
        memcpy(chcl->val, val, len);
        dill_trigger(&chcl->cl, 0);
        return 0;
    }
    /* The clause is not available immediately. */
    if(dill_slow(deadline == 0)) {errno = ETIMEDOUT; return -1;}
    /* Let's wait. */
    struct dill_chanclause chcl;
    dill_list_insert(&chcl.item, &ch->out);
    chcl.val = (void*)val;
    chcl.len = len;
    dill_waitfor(&chcl.cl, 0, dill_chcancel);
    struct dill_tmclause tmcl;
    dill_timer(&tmcl, 1, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == 1)) {errno = ETIMEDOUT; return -1;}
    if(dill_slow(errno != 0)) return -1;
    return 0;
}

int dill_chrecv(int h, void *val, size_t len, int64_t deadline) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Get the channel interface. */
    struct dill_halfchan *ch = dill_hquery(h, dill_halfchan_type);
    if(dill_slow(!ch)) return -1;
    /* Check whether the channel is done. */
    if(dill_slow(ch->done)) {errno = EPIPE; return -1;}
    /* If there's a sender waiting, copy the message directly
       from the sender. */
    if(!dill_list_empty(&ch->out)) {
        struct dill_chanclause *chcl = dill_cont(dill_list_next(&ch->out),
            struct dill_chanclause, item);
        if(dill_slow(len != chcl->len)) {
            dill_trigger(&chcl->cl, EMSGSIZE);
            errno = EMSGSIZE;
            return -1;
        }
        memcpy(val, chcl->val, len);
        dill_trigger(&chcl->cl, 0);
        return 0;
    }
    /* The clause is not immediately available. */
    if(dill_slow(deadline == 0)) {errno = ETIMEDOUT; return -1;}
    /* Let's wait. */
    struct dill_chanclause chcl;
    dill_list_insert(&chcl.item, &ch->in);
    chcl.val = val;
    chcl.len = len;
    dill_waitfor(&chcl.cl, 0, dill_chcancel);
    struct dill_tmclause tmcl;
    dill_timer(&tmcl, 1, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == 1)) {errno = ETIMEDOUT; return -1;}
    if(dill_slow(errno != 0)) return -1;
    return 0;
}

int dill_chdone(int h) {
    struct dill_halfchan *ch = dill_hquery(h, dill_halfchan_type);
    if(dill_slow(!ch)) return -1;
    /* Done is always done to the opposite side of the channel. */
    ch = dill_halfchan_other(ch);
    if(ch->done) {errno = EPIPE; return -1;}
    ch->done = 1;
    /* Resume any remaining senders and receivers on the channel
       with the EPIPE error. */
    while(!dill_list_empty(&ch->in)) {
        struct dill_chanclause *chcl = dill_cont(dill_list_next(&ch->in),
            struct dill_chanclause, item);
        dill_trigger(&chcl->cl, EPIPE);
    }
    while(!dill_list_empty(&ch->out)) {
        struct dill_chanclause *chcl = dill_cont(dill_list_next(&ch->out),
            struct dill_chanclause, item);
        dill_trigger(&chcl->cl, EPIPE);
    }
    return 0;
}

int dill_choose(struct dill_chclause *clauses, int nclauses, int64_t deadline) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    if(dill_slow(nclauses < 0 || (nclauses != 0 && !clauses))) {
        errno = EINVAL; return -1;}
    int i;
    for(i = 0; i != nclauses; ++i) {
        struct dill_chclause *cl = &clauses[i];
        struct dill_halfchan *ch = dill_hquery(cl->ch, dill_halfchan_type);
        if(dill_slow(!ch)) return i;
        if(dill_slow(cl->len > 0 && !cl->val)) {errno = EINVAL; return i;}
        struct dill_chanclause *chcl;
        switch(cl->op) {
        case DILL_CHSEND:
            ch = dill_halfchan_other(ch);
            if(dill_slow(ch->done)) {errno = EPIPE; return i;}
            if(dill_list_empty(&ch->in)) break;
            chcl = dill_cont(dill_list_next(&ch->in),
                struct dill_chanclause, item);
            if(dill_slow(cl->len != chcl->len)) {
                dill_trigger(&chcl->cl, EMSGSIZE);
                errno = EMSGSIZE;
                return i;
            }
            memcpy(chcl->val, cl->val, cl->len);
            dill_trigger(&chcl->cl, 0);
            errno = 0;
            return i;
        case DILL_CHRECV:
            if(dill_slow(ch->done)) {errno = EPIPE; return i;}
            if(dill_list_empty(&ch->out)) break;
            chcl = dill_cont(dill_list_next(&ch->out),
                struct dill_chanclause, item);
            if(dill_slow(cl->len != chcl->len)) {
                dill_trigger(&chcl->cl, EMSGSIZE);
                errno = EMSGSIZE;
                return i;
            }
            memcpy(cl->val, chcl->val, cl->len);
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
    struct dill_chanclause chcls[nclauses];
    for(i = 0; i != nclauses; ++i) {
        struct dill_halfchan *ch = dill_hquery(clauses[i].ch,
            dill_halfchan_type);
        dill_assert(ch);
        dill_list_insert(&chcls[i].item, clauses[i].op == DILL_CHRECV ?
            &ch->in : &dill_halfchan_other(ch)->out);
        chcls[i].val = clauses[i].val;
        chcls[i].len = clauses[i].len;
        dill_waitfor(&chcls[i].cl, i, dill_chcancel);
    }
    struct dill_tmclause tmcl;
    dill_timer(&tmcl, nclauses, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == nclauses)) {errno = ETIMEDOUT; return -1;}
    return id;
}

