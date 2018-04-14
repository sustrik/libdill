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

struct dill_hvfs {
    void *(*query)(struct dill_hvfs *vfs, const void *type);
    void (*close)(struct dill_hvfs *vfs);
};

DILL_EXPORT int dill_hmake(struct dill_hvfs *vfs);
DILL_EXPORT void *dill_hquery(int h, const void *type);

#if !defined DILL_DISABLE_RAW_NAMES
#define hvfs dill_hvfs
#define hmake dill_hmake
#define hquery dill_hquery
#endif

#if !defined DILL_DISABLE_SOCKETS

/******************************************************************************/
/*  Bytestream sockets.                                                       */
/******************************************************************************/

DILL_EXPORT extern const void *dill_bsock_type;

struct dill_bsock_vfs {
    int (*bsendl)(struct dill_bsock_vfs *vfs,
        struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
    int (*brecvl)(struct dill_bsock_vfs *vfs,
        struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
};

#if !defined DILL_DISABLE_RAW_NAMES
#define bsock_vfs dill_bsock_vfs
#define bsock_type dill_bsock_type
#endif

/******************************************************************************/
/*  Message sockets.                                                          */
/******************************************************************************/

DILL_EXPORT extern const void *dill_msock_type;

struct dill_msock_vfs {
    int (*msendl)(struct dill_msock_vfs *vfs,
        struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
    ssize_t (*mrecvl)(struct dill_msock_vfs *vfs,
        struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
};

#if !defined DILL_DISABLE_RAW_NAMES
#define msock_vfs dill_msock_vfs
#define msock_type dill_msock_type
#endif

#endif

#endif

