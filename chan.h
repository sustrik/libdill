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

#ifndef TS_CHAN_INCLUDED
#define TS_CHAN_INCLUDED

#include <stddef.h>

#include "debug.h"
#include "list.h"
#include "slist.h"

/* One of these structures is preallocated for every coroutine. */
struct ts_choosedata {
    /* List of clauses in the 'choose' statement. */
    struct ts_slist clauses;
    /* Deadline specified in 'deadline' clause. -1 if none. */
    int64_t ddline;
    /* Number of clauses that are immediately available. */
    int available;
};

/* Channel endpoint. */
struct ts_ep {
    /* Thanks to this flag we can cast from ep pointer to chan pointer. */
    enum {TS_SENDER, TS_RECEIVER} type;
    /* Sequence number of the choose operation being initialised. */
    int seqnum;
    /* Number of clauses referring to this endpoint within the choose
       operation being initialised. */
    int refs;
    /* Number of refs already processed. */
    int tmp;
    /* List of clauses waiting for this endpoint. */
    struct ts_list clauses;
};

/* Channel. */
struct ts_chan {
    /* The size of the elements stored in the channel, in bytes. */
    size_t sz;
    /* Channel holds two lists, the list of clauses waiting to send and list
       of clauses waiting to receive. */
    struct ts_ep sender;
    struct ts_ep receiver;
    /* Number of open handles to this channel. */
    int refcount;
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

    /* Debugging info. */
    struct ts_debug_chan debug;
};

/* This structure represents a single clause in a choose statement.
   Similarly, both chs() and chr() each create a single clause. */
struct ts_clause {
    /* Publicly visible members. */
    struct ts_chan *channel;
    int op;
    void *val;
    size_t len;
    /* Member of list of clauses waiting for a channel endpoint. */
    struct ts_list_item epitem;
    /* Linked list of clauses in the choose statement. */
    struct ts_slist_item chitem;
    /* The coroutine which created the clause. */
    struct ts_cr *cr;
    /* Channel endpoint the clause is waiting for. */
    struct ts_ep *ep;
    /* The index to jump to when the clause is executed. */
    int idx;
    /* If 0, there's no peer waiting for the clause at the moment.
       If 1, there is one. */
    int available;
    /* If 1, the clause is in the list of channel's senders/receivers. */
    int used;
};

#endif

