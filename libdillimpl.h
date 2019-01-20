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
DILL_EXPORT int dill_hswap(int h1, int h2);
DILL_EXPORT int dill_hnullify(int h);
DILL_EXPORT void *dill_hquery(int h, const void *type);

#if !defined DILL_DISABLE_RAW_NAMES
#define hvfs dill_hvfs
#define hmake dill_hmake
#define hswap dill_hswap
#define hnullify dill_hnullify
#define hquery dill_hquery
#endif

#if !defined DILL_DISABLE_SOCKETS

/******************************************************************************/
/*  Bytestream sockets.                                                       */
/******************************************************************************/

DILL_EXPORT extern const void *dill_bsock_type;

struct dill_bsock_vfs {
    int (*bsend)(struct dill_bsock_vfs *bvfs,
        const void *buf, size_t len, int64_t deadline);
    int (*brecv)(struct dill_bsock_vfs *bvfs,
        void *buf, size_t len, int64_t deadline);
};

#if !defined DILL_DISABLE_RAW_NAMES
#define bsock_type dill_bsock_type
#define bsock_vfs dill_bsock_vfs
#endif

/******************************************************************************/
/*  Message sockets.                                                          */
/******************************************************************************/

DILL_EXPORT extern const void *dill_msock_type;

struct dill_msock_vfs {
    int (*msend)(struct dill_msock_vfs *mvfs,
        const void *buf, size_t len, int64_t deadline);
    ssize_t (*mrecv)(struct dill_msock_vfs *mvfs,
        void *buf, size_t len, int64_t deadline);
};

#if !defined DILL_DISABLE_RAW_NAMES
#define msock_type dill_msock_type
#define msock_vfs dill_msock_vfs
#endif

#endif

#endif

