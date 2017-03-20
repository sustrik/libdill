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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libdillimpl.h"
#include "utils.h"

dill_unique_id(mtrace_type);

static void *mtrace_hquery(struct hvfs *hvfs, const void *type);
static void mtrace_hclose(struct hvfs *hvfs);
static int mtrace_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t mtrace_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

struct mtrace_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    char name[256];
};

static void *mtrace_hquery(struct hvfs *hvfs, const void *type) {
    struct mtrace_sock *obj = (struct mtrace_sock*)hvfs;
    if(type == msock_type) return &obj->mvfs;
    if(type == mtrace_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int mtrace_attach(int s, const char *name) {
    if(dill_slow(strlen(name) > 255)) {errno = ENAMETOOLONG; return -1;}
    /* Check whether underlying socket is message-based. */
    if(dill_slow(!hquery(s, msock_type))) return -1;
    /* Create the object. */
    struct mtrace_sock *obj = malloc(sizeof(struct mtrace_sock));
    if(dill_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = mtrace_hquery;
    obj->hvfs.close = mtrace_hclose;
    obj->mvfs.msendl = mtrace_msendl;
    obj->mvfs.mrecvl = mtrace_mrecvl;
    obj->u = s;
    strcpy(obj->name, name);
    /* Create the handle. */
    int h = hmake(&obj->hvfs);
    if(dill_slow(h < 0)) {
        int err = errno;
        free(obj);
        errno = err;
        return -1;
    }
    return h;
}

int mtrace_detach(int s) {
    struct mtrace_sock *obj = hquery(s, mtrace_type);
    if(dill_slow(!obj)) return -1;
    int u = obj->u;
    free(obj);
    return u;
}

static int mtrace_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct mtrace_sock *obj = dill_cont(mvfs, struct mtrace_sock, mvfs);
    size_t len = 0;
    fprintf(stderr, "msend(%s, 0x", obj->name);
    struct iolist *it = first;
    while(it) {
        int i;
        for(i = 0; i != it->iol_len; ++i)
            fprintf(stderr, "%02x", (int)((uint8_t*)it->iol_base)[i]);
        len += it->iol_len;
        it = it->iol_next;
    }
    fprintf(stderr, ", %zu)\n", len);
    return msendl(obj->u, first, last, deadline);
}

static ssize_t mtrace_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct mtrace_sock *obj = dill_cont(mvfs, struct mtrace_sock, mvfs);
    ssize_t sz = mrecvl(obj->u, first, last, deadline);
    if(dill_slow(sz < 0)) return -1;
    fprintf(stderr, "mrecv(%s, 0x", obj->name);
    size_t toprint = sz;
    struct iolist *it = first;
    while(it && toprint) {
        int i;
        for(i = 0; i != it->iol_len && toprint; ++i) {
            fprintf(stderr, "%02x", (int)((uint8_t*)it->iol_base)[i]);
            --toprint;
        }
        it = it->iol_next;
    }
    fprintf(stderr, ", %zu)\n", (size_t)sz);
    return sz;
}

static void mtrace_hclose(struct hvfs *hvfs) {
    struct mtrace_sock *obj = (struct mtrace_sock*)hvfs;
    int rc = hclose(obj->u);
    dill_assert(rc == 0);
    free(obj);
}

