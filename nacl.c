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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tweetnacl/tweetnacl.h"

#include "libdillimpl.h"
#include "iol.h"
#include "utils.h"

dill_unique_id(nacl_type);

static void *nacl_hquery(struct hvfs *hvfs, const void *type);
static void nacl_hclose(struct hvfs *hvfs);
static int nacl_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t nacl_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

#define NACL_EXTRABYTES \
    (crypto_secretbox_ZEROBYTES + crypto_secretbox_NONCEBYTES)

struct nacl_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int s;
    size_t buflen;
    uint8_t *buf1;
    uint8_t *buf2;
    uint8_t key[crypto_secretbox_KEYBYTES];
};

static void *nacl_hquery(struct hvfs *hvfs, const void *type) {
    struct nacl_sock *obj = (struct nacl_sock*)hvfs;
    if(type == msock_type) return &obj->mvfs;
    if(type == nacl_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int nacl_attach(int s, const void *key, size_t keylen, int64_t deadline) {
    int err;
    if(dill_slow(!key || keylen != crypto_secretbox_KEYBYTES)) {
        err = EINVAL; goto error2;}
    /* Check whether underlying socket is message-based. */
    if(dill_slow(!hquery(s, msock_type))) {err = errno; goto error1;}
    /* Create the object. */
    struct nacl_sock *obj = malloc(sizeof(struct nacl_sock));
    if(dill_slow(!obj)) {errno = ENOMEM; goto error1;}
    obj->hvfs.query = nacl_hquery;
    obj->hvfs.close = nacl_hclose;
    obj->mvfs.msendl = nacl_msendl;
    obj->mvfs.mrecvl = nacl_mrecvl;
    obj->s = s;
    obj->buflen = 0;
    obj->buf1 = NULL;
    obj->buf2 = NULL;
    memcpy(obj->key, key, crypto_secretbox_KEYBYTES);
    /* Create the handle. */
    int h = hmake(&obj->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    /* Make a private copy of the underlying socket. */
    obj->s = hdup(s);
    if(dill_slow(obj->s < 0)) {err = errno; goto error3;}
    int rc = hclose(s);
    dill_assert(rc == 0);
    return h;
error3:
    rc = hclose(h);
    dill_assert(rc == 0);
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int nacl_detach(int s) {
    struct nacl_sock *obj = hquery(s, nacl_type);
    if(dill_slow(!obj)) return -1;
    free(obj->buf1);
    free(obj->buf2);
    int u = obj->s;
    free(obj);
    return u;
}

static int nacl_resizebufs(struct nacl_sock *obj, size_t len) {
   if(dill_slow(!obj->buf1 || obj->buflen < len)) {
        obj->buflen = len;
        obj->buf1 = realloc(obj->buf1, len);
        if(dill_slow(!obj->buf1)) {errno = ENOMEM; return -1;}
        obj->buf2 = realloc(obj->buf2, len);
        if(dill_slow(!obj->buf2)) {errno = ENOMEM; return -1;}
    }
    return 0;
}

static int nacl_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct nacl_sock *obj = dill_cont(mvfs, struct nacl_sock, mvfs);
    size_t len;
    int rc = iol_check(first, last, NULL, &len);
    if(dill_slow(rc < 0)) return -1;
    /* If needed, adjust the buffers. */
    rc = nacl_resizebufs(obj, NACL_EXTRABYTES + len);
    if(dill_slow(rc < 0)) return -1;
    /* Generate random nonce. */
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    rc = dill_random(nonce, crypto_secretbox_NONCEBYTES); 
    if(dill_slow(rc != 0)) return -1;
    /* Encrypt and authenticate the message. */
    size_t mlen = len + crypto_secretbox_ZEROBYTES;
    memset(obj->buf1, 0, crypto_secretbox_ZEROBYTES);
    uint8_t *pos = obj->buf1 + crypto_secretbox_ZEROBYTES;
    struct iolist *it;
    for(it = first; it; it = it->iol_next) {
        memcpy(pos, it->iol_base, it->iol_len);
        pos += it->iol_len;
    }
    crypto_secretbox(obj->buf2, obj->buf1, mlen, nonce, obj->key);
    /* Prepare the message: nonce + ciphertext */
    memcpy(obj->buf1, nonce, crypto_secretbox_NONCEBYTES);
    memcpy(obj->buf1 + crypto_secretbox_NONCEBYTES,
        obj->buf2 + crypto_secretbox_BOXZEROBYTES,
        mlen - crypto_secretbox_BOXZEROBYTES);
    /* Send the the encrypted message. */
    return msend(obj->s, obj->buf1, crypto_secretbox_NONCEBYTES + mlen -
        crypto_secretbox_BOXZEROBYTES , deadline);
}

static ssize_t nacl_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct nacl_sock *obj = dill_cont(mvfs, struct nacl_sock, mvfs);
    size_t len;
    int rc = iol_check(first, last, NULL, &len);
    if(dill_slow(rc < 0)) return -1;
    /* If needed, adjust the buffers. */
    rc = nacl_resizebufs(obj, NACL_EXTRABYTES + len);
    /* Read the encrypted message. */
    ssize_t sz = mrecv(obj->s, obj->buf1, NACL_EXTRABYTES + len, deadline);
    if(dill_slow(sz < 0)) return -1;
    if(sz > NACL_EXTRABYTES + len) {errno = EMSGSIZE; return -1;}
    /* Store the nonce. */
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    memcpy(nonce, obj->buf1, crypto_secretbox_NONCEBYTES);
    /* Decrypt and authenticate the message. */
    size_t clen = crypto_secretbox_BOXZEROBYTES +
        (sz - crypto_secretbox_NONCEBYTES);
    memset(obj->buf2, 0, crypto_secretbox_BOXZEROBYTES);
    memcpy(obj->buf2 + crypto_secretbox_BOXZEROBYTES,
        obj->buf1 + crypto_secretbox_NONCEBYTES,
        clen - crypto_secretbox_BOXZEROBYTES);
    rc = crypto_secretbox_open(obj->buf1, obj->buf2, clen, nonce, obj->key);
    if(dill_slow(rc < 0)) {errno = EACCES; return -1;}
    /* Copy the message into user's buffer. */
    sz = clen - crypto_secretbox_ZEROBYTES;
    uint8_t *pos = obj->buf1 + crypto_secretbox_ZEROBYTES;
    struct iolist *it = first;
    while(1) {
        size_t tocopy = sz < it->iol_len ? sz : it->iol_len;
        if(it->iol_base) memcpy(it->iol_base, pos, tocopy);
        sz -= tocopy;
        if(sz == 0) break;
        pos += it->iol_len;
        it = it->iol_next;
    }

    return clen - crypto_secretbox_ZEROBYTES;
}

static void nacl_hclose(struct hvfs *hvfs) {
    struct nacl_sock *obj = (struct nacl_sock*)hvfs;
    if(dill_fast(obj->s >= 0)) {
        int rc = hclose(obj->s);
        dill_assert(rc == 0);
    }
    free(obj->buf1);
    free(obj->buf2);
    free(obj);
}

