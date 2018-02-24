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
#include <libdillimpl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "iol.h"
#include "utils.h"

int wsraw_request_key(char *request_key) {
    if(dill_slow(!request_key)) {errno = EINVAL; return -1;}
    uint8_t nonce[16];
    int rc = dill_random(nonce, sizeof(nonce));
    if(dill_slow(rc < 0)) return -1;
    rc = dill_base64_encode(nonce, sizeof(nonce), request_key, WSRAW_KEY_SIZE);
    if(dill_slow(rc < 0)) {errno = EFAULT; return -1;}
    return 0;
}

int wsraw_response_key(const char *request_key, char *response_key) {
    if(dill_slow(!request_key)) {errno = EINVAL; return -1;}
    if(dill_slow(!response_key)) {errno = EINVAL; return -1;}
    struct dill_sha1 sha1;
    dill_sha1_init(&sha1);
    int i;
    /* The key sent in the original request. */
    for(i = 0; request_key[i] != 0; ++i)
        dill_sha1_hashbyte(&sha1, request_key[i]);
    /* RFC 6455-specified UUID. */
    const char *uuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    for(i = 0; uuid[i] != 0; ++i) dill_sha1_hashbyte(&sha1, uuid[i]);
    /* Convert the SHA1 to Base64. */
    int rc = dill_base64_encode(dill_sha1_result(&sha1), DILL_SHA1_HASH_LEN,
        response_key, WSRAW_KEY_SIZE);
    if(dill_slow(rc < 0)) {errno = EFAULT; return -1;}
    return 0;
}

dill_unique_id(wsraw_type);

struct wsraw_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    unsigned int server : 1;
    unsigned int type : 1;
    unsigned int mem : 1;
};

DILL_CT_ASSERT(WSRAW_SIZE >= sizeof(struct wsraw_sock));

static void *wsraw_hquery(struct hvfs *hvfs, const void *type);
static void wsraw_hclose(struct hvfs *hvfs);
static int wsraw_hdone(struct hvfs *hvfs, int64_t deadline);
static int wsraw_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t wsraw_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

static void *wsraw_hquery(struct hvfs *hvfs, const void *type) {
    struct wsraw_sock *self = (struct wsraw_sock*)hvfs;
    if(type == msock_type) return &self->mvfs;
    if(type == wsraw_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int wsraw_attach_client_mem(int s, int type, void *mem, int64_t deadline) {
    if(dill_slow(!mem)) {errno = EINVAL; return -1;}
    if(dill_slow(type != WSRAW_TYPE_BINARY && type != WSRAW_TYPE_TEXT)) {
        errno = EINVAL; return -1;}
    /* Check whether underlying socket is a bytestream. */
    if(dill_slow(!hquery(s, bsock_type))) return -1;
    /* Create the object. */
    struct wsraw_sock *self = (struct wsraw_sock*)mem;
    self->hvfs.query = wsraw_hquery;
    self->hvfs.close = wsraw_hclose;
    self->hvfs.done = wsraw_hdone;
    self->mvfs.msendl = wsraw_msendl;
    self->mvfs.mrecvl = wsraw_mrecvl;
    self->u = s;
    self->server = 0;
    self->type = type;
    self->mem = 1;
    /* Create the handle. */
    return hmake(&self->hvfs);
}

int wsraw_attach_client(int s, int type, int64_t deadline) {
    int err;
    struct wsraw_sock *obj = malloc(sizeof(struct wsraw_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int ws = wsraw_attach_client_mem(s, type, obj, deadline);
    if(dill_slow(ws < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return ws;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int wsraw_attach_server_mem(int s, int type, void *mem, int64_t deadline) {
    if(dill_slow(!mem)) {errno = EINVAL; return -1;}
    if(dill_slow(type != WSRAW_TYPE_BINARY && type != WSRAW_TYPE_TEXT)) {
        errno = EINVAL; return -1;}
    /* Check whether underlying socket is a bytestream. */
    if(dill_slow(!hquery(s, bsock_type))) return -1;
    /* Create the object. */
    struct wsraw_sock *self = (struct wsraw_sock*)mem;
    self->hvfs.query = wsraw_hquery;
    self->hvfs.close = wsraw_hclose;
    self->hvfs.done = wsraw_hdone;
    self->mvfs.msendl = wsraw_msendl;
    self->mvfs.mrecvl = wsraw_mrecvl;
    self->u = s;
    self->server = 1;
    self->type = type;
    self->mem = 1;
    /* Create the handle. */
    return hmake(&self->hvfs);
}

int wsraw_attach_server(int s, int type, int64_t deadline) {
    int err;
    struct wsraw_sock *obj = malloc(sizeof(struct wsraw_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int ws = wsraw_attach_server_mem(s, type, obj, deadline);
    if(dill_slow(ws < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return ws;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

static int wsraw_msendl_(struct msock_vfs *mvfs, int type,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct wsraw_sock *self = dill_cont(mvfs, struct wsraw_sock, mvfs);
    size_t len;
    int rc = iol_check(first, last, NULL, &len);
    if(dill_slow(rc < 0)) return -1;
    uint8_t buf[12];
    size_t sz;
    buf[0] = type == WSRAW_TYPE_BINARY ? 0x82 : 0x81;
    if(len > 0xffff) {
        buf[1] = 127;
        dill_putll(buf + 2, len);
        sz = 10;
    }
    else if(len > 125) {
        buf[1] = 126;
        dill_puts(buf + 2, len);
        sz = 4;
    }
    else {
        buf[1] = (uint8_t)len;
        sz = 2;
    }
    uint8_t mask[4];
    if(!self->server) {
        rc = dill_random(mask, sizeof(mask));
        if(dill_slow(rc < 0)) return -1;
        buf[1] |= 0x80;
        memcpy(buf + sz, mask, 4);
        sz += 4;
    }
    rc = bsend(self->u, buf, sz, deadline);
    if(dill_slow(rc < 0)) return -1;
    /* On the server side we can send the payload as is. */
    if(self->server) {
        rc = bsendl(self->u, first, last, deadline);
        if(dill_slow(rc < 0)) return -1;
        return 0;
    }
    /* On the client side, the payload has to be masked. */
    size_t pos = 0;
    while(first) {
        size_t i;
        for(i = 0; i != first->iol_len; ++i) {
            uint8_t b = ((uint8_t*)first->iol_base)[i] ^
                mask[pos++ % sizeof(mask)];
            rc = bsend(self->u, &b, 1, deadline);
            if(dill_slow(rc < 0)) return -1;
        }
        first = first->iol_next;
    }
    return 0;
}

static ssize_t wsraw_mrecvl_(struct msock_vfs *mvfs, int *type,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct wsraw_sock *self = dill_cont(mvfs, struct wsraw_sock, mvfs);
    int rc = iol_check(first, last, NULL, NULL);
    if(dill_slow(rc < 0)) return -1;
    size_t res = 0;
    /* Message may consist of multiple frames. Read them one by one. */
    struct iolist it = *first;
    while(1) {
        uint8_t hdr1[2];
        rc = brecv(self->u, hdr1, sizeof(hdr1), deadline);
        if(dill_slow(rc < 0)) return -1;
        if(hdr1[0] & 0x70) {errno = EPROTO; return -1;}
        int opcode = hdr1[0] & 0x0f;
        /* TODO: Other opcodes. */
        switch(opcode) {
        case 1:
            if(type) *type = WSRAW_TYPE_TEXT;
            break;
        case 2:
            if(type) *type = WSRAW_TYPE_BINARY;
            break;
        default:
            errno = EPROTO;
            return -1;
        }
        if(!self->server ^ !(hdr1[1] & 0x80)) {errno = EPROTO; return -1;}
        size_t sz = hdr1[1] & 0x7f;
        if(sz == 126) {
            uint8_t hdr2[2];
            rc = brecv(self->u, hdr2, sizeof(hdr2), deadline);
            if(dill_slow(rc < 0)) return -1;
            sz = dill_gets(hdr2);
        }
        else if(sz == 127) {
            uint8_t hdr2[8];
            rc = brecv(self->u, hdr2, sizeof(hdr2), deadline);
            if(dill_slow(rc < 0)) return -1;
            sz = dill_getll(hdr2);
        }
        res += sz;
        uint8_t mask[4];
        if(self->server) {
            rc = brecv(self->u, mask, sizeof(mask), deadline);
            if(dill_slow(rc < 0)) return -1;
        }
        /* Frame may be read into multiple iolist elements. */
        while(1) {
            size_t toread = sz < it.iol_len ? sz : it.iol_len;
            if(toread > 0) {
                rc = brecv(self->u, it.iol_base, toread, deadline);
                if(dill_slow(rc < 0)) return -1;
                if(self->server) {
                    size_t i;
                    for(i = 0; i != toread; ++i)
                        ((uint8_t*)it.iol_base)[i] ^= mask[i % 4];
                }
            }
            sz -= it.iol_len;
            if(sz == 0) break;
            if(dill_slow(!it.iol_next)) {errno = EMSGSIZE; return -1;}
            it = *it.iol_next;
        }
        if(hdr1[0] & 0x80) break;
    }
    return res;
}

int wsraw_send(int s, int type, const void *buf, size_t len, int64_t deadline) {
    struct wsraw_sock *self = hquery(s, wsraw_type);
    if(dill_slow(!self)) return -1;
    struct iolist iol = {(void*)buf, len, NULL, 0};
    return wsraw_msendl_(&self->mvfs, type, &iol, &iol, deadline);
}

ssize_t wsraw_recv(int s, int *type, void *buf, size_t len, int64_t deadline) {
    struct wsraw_sock *self = hquery(s, wsraw_type);
    if(dill_slow(!self)) return -1;
    struct iolist iol = {(void*)buf, len, NULL, 0};
    return wsraw_mrecvl_(&self->mvfs, type, &iol, &iol, deadline);
}

int wsraw_sendl(int s, int type, struct iolist *first, struct iolist *last,
      int64_t deadline) {
    struct wsraw_sock *self = hquery(s, wsraw_type);
    if(dill_slow(!self)) return -1;
    return wsraw_msendl_(&self->mvfs, type, first, last, deadline);
}

ssize_t wsraw_recvl(int s, int *type, struct iolist *first, struct iolist *last,
      int64_t deadline) {
    struct wsraw_sock *self = hquery(s, wsraw_type);
    if(dill_slow(!self)) return -1;
    return wsraw_mrecvl_(&self->mvfs, type, first, last, deadline);
}

static int wsraw_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
   struct wsraw_sock *self = dill_cont(mvfs, struct wsraw_sock, mvfs);
   return wsraw_msendl_(mvfs, self->type, first, last, deadline);
}

static ssize_t wsraw_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
   struct wsraw_sock *self = dill_cont(mvfs, struct wsraw_sock, mvfs);
   int type;
   ssize_t sz = wsraw_mrecvl_(mvfs, &type, first, last, deadline);
   if(dill_slow(sz < 0)) return -1;
   if(dill_slow(type != self->type)) {errno = EPROTO; return -1;}
   return sz;
}

static int wsraw_hdone(struct hvfs *hvfs, int64_t deadline) {
    dill_assert(0);
}

int wsraw_detach(int s, int64_t deadline) {
    dill_assert(0);
}

static void wsraw_hclose(struct hvfs *hvfs) {
    dill_assert(0);
}

