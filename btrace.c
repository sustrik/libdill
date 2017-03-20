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

dill_unique_id(btrace_type);

static void *btrace_hquery(struct hvfs *hvfs, const void *type);
static void btrace_hclose(struct hvfs *hvfs);
static int btrace_bsendl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static int btrace_brecvl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

struct btrace_sock {
    struct hvfs hvfs;
    struct bsock_vfs bvfs;
    int u;
    char name[256];
};

static void *btrace_hquery(struct hvfs *hvfs, const void *type) {
    struct btrace_sock *obj = (struct btrace_sock*)hvfs;
    if(type == bsock_type) return &obj->bvfs;
    if(type == btrace_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int btrace_attach(int s, const char *name) {
     if(dill_slow(strlen(name) > 255)) {errno = ENAMETOOLONG; return -1;}
    /* Check whether underlying socket is a bytestream. */
    if(dill_slow(!hquery(s, bsock_type))) return -1;
    /* Create the object. */
    struct btrace_sock *obj = malloc(sizeof(struct btrace_sock));
    if(dill_slow(!obj)) {errno = ENOMEM; return -1;}
    obj->hvfs.query = btrace_hquery;
    obj->hvfs.close = btrace_hclose;
    obj->bvfs.bsendl = btrace_bsendl;
    obj->bvfs.brecvl = btrace_brecvl;
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

int btrace_detach(int s) {
    struct btrace_sock *obj = hquery(s, btrace_type);
    if(dill_slow(!obj)) return -1;
    int u = obj->u;
    free(obj);
    return u;
}

static int btrace_bsendl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct btrace_sock *obj = dill_cont(bvfs, struct btrace_sock, bvfs);
    size_t len = 0;
    fprintf(stderr, "bsend(%s, 0x", obj->name);
    struct iolist *it = first;
    while(it) {
        int i;
        for(i = 0; i != it->iol_len; ++i)
            fprintf(stderr, "%02x", (int)((uint8_t*)it->iol_base)[i]);
        len += it->iol_len;
        it = it->iol_next;
    }
    fprintf(stderr, ", %zu)\n", len);
    return bsendl(obj->u, first, last, deadline);
}

static int btrace_brecvl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct btrace_sock *obj = dill_cont(bvfs, struct btrace_sock, bvfs);
    int rc = brecvl(obj->u, first, last, deadline);
    if(dill_slow(rc < 0)) return -1;
    size_t len = 0;
    fprintf(stderr, "brecv(%s, 0x", obj->name);
    struct iolist *it = first;
    while(it) {
        int i;
        for(i = 0; i != it->iol_len; ++i)
            fprintf(stderr, "%02x", (int)((uint8_t*)it->iol_base)[i]);
        len += it->iol_len;
        it = it->iol_next;
    }
    fprintf(stderr, ", %zu)\n", len);
    return 0;
}

static void btrace_hclose(struct hvfs *hvfs) {
    struct btrace_sock *obj = (struct btrace_sock*)hvfs;
    int rc = hclose(obj->u);
    dill_assert(rc == 0);
    free(obj);
}

