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
#include <string.h>

#include "cr.h"
#include "libdill.h"
#include "list.h"
#include "poller.h"
#include "utils.h"

/* The channl. Memory layout of a channel looks like this:
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

/******************************************************************************/
/*  Handle implementation.                                                    */
/******************************************************************************/

static const int dill_chan_type_placeholder = 0;
static const void *dill_chan_type = &dill_chan_type_placeholder;

static void dill_chan_close(int h);
static void dill_chan_dump(int h) {dill_assert(0);}

static const struct hvfptrs dill_chan_vfptrs = {
    dill_chan_close,
    dill_chan_dump
};

/******************************************************************************/
/*  Channel creation and deallocation.                                        */
/******************************************************************************/

int dill_channel(size_t itemsz, size_t bufsz, const char *created) {
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

int dill_chsend(int h, const void *val, size_t len, int64_t deadline,
      const char *current) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    struct dill_chan *ch = hdata(h, dill_chan_type);
    if(dill_slow(!ch)) return -1;
    if(dill_slow(len != ch->sz || (len > 0 && !val))) {
        errno = EINVAL; return -1;}
    if(dill_list_empty(&ch->in)) {
        if(ch->items < ch->bufsz) {
            /* Write the item to the buffer. */
            size_t pos = (ch->first + ch->items) % ch->bufsz;
            memcpy(((char*)(ch + 1)) + (pos * ch->sz) , val, len);
            ++ch->items;
            return 0;
        }
        /* Block. */
        if(dill_slow(deadline == 0)) {errno == ETIMEDOUT; return -1;}
        struct dill_chcl chcl;
        struct dill_tmcl tmcl;
        chcl.val = (void*)val;
        dill_waitfor(&chcl.cl, 1, &ch->out, NULL);
        if(deadline > 0)
            dill_addtimer(&tmcl, 2, deadline);
        int id = dill_wait();
        if(dill_slow(id < 0)) return -1;
        if(dill_slow(id == 2)) {errno = ETIMEDOUT; return -1;}
        dill_assert(id == 1);
        return errno == 0 ? 0 : -1;
    }
    /* Copy the message directly to the waiting receiver. */
    struct dill_chcl *chcl = dill_cont(dill_list_begin(&ch->in),
        struct dill_chcl, cl.epitem);
    memcpy(chcl->val, val, len);
    dill_trigger(&chcl->cl, 0);
    return 0;
}

int dill_chrecv(int h, void *val, size_t len, int64_t deadline,
      const char *current) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    struct dill_chan *ch = hdata(h, dill_chan_type);
    if(dill_slow(!ch)) return -1;
    if(dill_slow(len != ch->sz || (len > 0 && !val))) {
        errno = EINVAL; return -1;}
    if(ch->items) {        
        /* Read an item from the buffer. */
        memcpy(val, ((char*)(ch + 1)) + (ch->first * ch->sz), ch->sz);
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
        return 0;
    }
    if(!dill_list_empty(&ch->out)) {
        /* Copy the message directly from a waiting sender. */
        struct dill_chcl *chcl = dill_cont(dill_list_begin(&ch->out),
            struct dill_chcl, cl.epitem);
        memcpy(val, chcl->val, ch->sz);
        dill_trigger(&chcl->cl, 0);
        return 0;
    }
    /* Block. */
    if(dill_slow(deadline == 0)) {errno == ETIMEDOUT; return -1;}
    struct dill_chcl chcl;
    struct dill_tmcl tmcl;
    chcl.val = val;
    dill_waitfor(&chcl.cl, 1, &ch->in, NULL);
    if(deadline > 0)
        dill_addtimer(&tmcl, 2, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == 2)) {errno = ETIMEDOUT; return -1;}
    dill_assert(id == 1);
    return errno == 0 ? 0 : -1;
}

int dill_chdone(int ch, const char *current) {
    dill_assert(0);
}

