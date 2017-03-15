/*

  Copyright (c) 2017 Martin Sustrik

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

#ifndef LIBDILLIMPL_H_INCLUDED
#define LIBDILLIMPL_H_INCLUDED

#include "libdill.h"

/******************************************************************************/
/*  Handles                                                                   */
/******************************************************************************/

struct hvfs {
    void *(*query)(struct hvfs *vfs, const void *type);
    void (*close)(struct hvfs *vfs);
    int (*done)(struct hvfs *vfs, int64_t deadline);
    /* Reserved. Do not use directly! */
    unsigned int refcount;
};

DILL_EXPORT int hmake(struct hvfs *vfs);
DILL_EXPORT void *hquery(int h, const void *type);

#if !defined DILL_DISABLE_SOCKETS

/******************************************************************************/
/*  Bytestream sockets.                                                       */
/******************************************************************************/

DILL_EXPORT extern const void *bsock_type;

struct bsock_vfs {
    int (*bsendl)(struct bsock_vfs *vfs,
        struct iolist *first, struct iolist *last, int64_t deadline);
    int (*brecvl)(struct bsock_vfs *vfs,
        struct iolist *first, struct iolist *last, int64_t deadline);
};

/******************************************************************************/
/*  Message sockets.                                                          */
/******************************************************************************/

DILL_EXPORT extern const void *msock_type;

struct msock_vfs {
    int (*msendl)(struct msock_vfs *vfs,
        struct iolist *first, struct iolist *last, int64_t deadline);
    ssize_t (*mrecvl)(struct msock_vfs *vfs,
        struct iolist *first, struct iolist *last, int64_t deadline);
};

#endif

#endif

