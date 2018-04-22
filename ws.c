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
#include <stdlib.h>
#include <string.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "iol.h"
#include "utils.h"

dill_unique_id(dill_ws_type);

struct dill_ws_sock {
    struct dill_hvfs hvfs;
    struct dill_msock_vfs mvfs;
    int u;
    int flags;
    unsigned int indone : 1;
    unsigned int outdone: 1;
    unsigned int server : 1;
    unsigned int mem : 1;
    uint16_t status;
    uint8_t msglen;
    uint8_t msg[128];
};

DILL_CHECK_STORAGE(dill_ws_sock, dill_ws_storage)

static void *dill_ws_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_ws_hclose(struct dill_hvfs *hvfs);
static int dill_ws_msendl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
static ssize_t dill_ws_mrecvl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);

static void *dill_ws_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_ws_sock *self = (struct dill_ws_sock*)hvfs;
    if(type == dill_msock_type) return &self->mvfs;
    if(type == dill_ws_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int dill_ws_attach_client_mem(int s, int flags, const char *resource,
      const char *host, struct dill_ws_storage *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error;}
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) {err = errno; goto error;}
    struct dill_ws_sock *self = (struct dill_ws_sock*)mem;
    /* Take ownership of the underlying socket. */
    s = dill_hown(s);
    if(dill_slow(s < 0)) {err = errno; goto error;}
    self->hvfs.query = dill_ws_hquery;
    self->hvfs.close = dill_ws_hclose;
    self->mvfs.msendl = dill_ws_msendl;
    self->mvfs.mrecvl = dill_ws_mrecvl;
    self->u = s;
    self->flags = flags;
    self->indone = 0;
    self->outdone = 0;
    self->server = 0;
    self->mem = 1;
    self->status = 0;
    self->msglen = 0;
    if(flags & DILL_WS_NOHTTP) {
        int h = dill_hmake(&self->hvfs);
        if(dill_slow(h < 0)) {err = errno; goto error;}
        return h;
    }
    if(dill_slow(!resource || !host)) {err = EINVAL; goto error;}
    struct dill_http_storage http_mem;
    s = dill_http_attach_mem(s, &http_mem);
    if(dill_slow(s < 0)) {err = errno; goto error;}
    /* Send HTTP request. */
    int rc = dill_http_sendrequest(s, "GET", resource, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_http_sendfield(s, "Host", host, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_http_sendfield(s, "Upgrade", "websocket", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_http_sendfield(s, "Connection", "Upgrade", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    char request_key[WS_KEY_SIZE];
    rc = dill_ws_request_key(request_key);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_http_sendfield(s, "Sec-WebSocket-Key", request_key, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_http_sendfield(s, "Sec-WebSocket-Version", "13", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    /* TODO: Protocol, Extensions? */
    rc = dill_http_done(s, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}

    /* Receive HTTP response from the server. */
    char reason[256];
    rc = dill_http_recvstatus(s, reason, sizeof(reason), deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    if(dill_slow(rc != 101)) {errno = EPROTO; return -1;}
    int has_upgrade = 0;
    int has_connection = 0;
    int has_key = 0;
    while(1) {
        char name[256];
        char value[256];
        rc = dill_http_recvfield(s, name, sizeof(name), value, sizeof(value),
            deadline);
        if(rc < 0 && errno == EPIPE) break;
        if(dill_slow(rc < 0)) {err = errno; goto error;}
        if(strcasecmp(name, "Upgrade") == 0) {
           if(has_upgrade || strcasecmp(value, "websocket") != 0) {
               err = EPROTO; goto error;}
           has_upgrade = 1;
           continue;
        }
        if(strcasecmp(name, "Connection") == 0) {
           if(has_connection || strcasecmp(value, "Upgrade") != 0) {
               err = EPROTO; goto error;}
           has_connection = 1;
           continue;
        }
        if(strcasecmp(name, "Sec-WebSocket-Accept") == 0) {
            if(has_key) {err = EPROTO; goto error;}
            char response_key[WS_KEY_SIZE];
            rc = dill_ws_response_key(request_key, response_key);
            if(dill_slow(rc < 0)) {err = errno; goto error;}
            if(dill_slow(strcmp(value, response_key) != 0)) {
                err = EPROTO; goto error;}
            has_key = 1;
            continue;
        }
    }
    if(!has_upgrade || !has_connection || !has_key) {err = EPROTO; goto error;}

    s = dill_http_detach(s, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error;}
    self->u = s;
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error;}
    return h;
error:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_ws_attach_client(int s, int flags, const char *resource,
      const char *host, int64_t deadline) {
    int err;
    struct dill_ws_sock *obj = malloc(sizeof(struct dill_ws_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    s = dill_ws_attach_client_mem(s, flags, resource, host,
        (struct dill_ws_storage*)obj, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_ws_attach_server_mem(int s, int flags,
      char *resource, size_t resourcelen,
      char *host, size_t hostlen,
      struct dill_ws_storage *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error;}
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) {err = errno; goto error;}
    struct dill_ws_sock *self = (struct dill_ws_sock*)mem;
    /* Take ownership of the underlying socket. */
    s = dill_hown(s);
    if(dill_slow(s < 0)) {err = errno; goto error;}
    self->hvfs.query = dill_ws_hquery;
    self->hvfs.close = dill_ws_hclose;
    self->mvfs.msendl = dill_ws_msendl;
    self->mvfs.mrecvl = dill_ws_mrecvl;
    self->u = s;
    self->flags = flags;
    self->indone = 0;
    self->outdone = 0;
    self->server = 1;
    self->mem = 1;
    self->status = 0;
    self->msglen = 0;
    if(flags & DILL_WS_NOHTTP) {
        int h = dill_hmake(&self->hvfs);
        if(dill_slow(h < 0)) {err = errno; goto error;}
        return h;
    }
    struct dill_http_storage http_mem;
    s = dill_http_attach_mem(s, &http_mem);
    if(dill_slow(s < 0)) {err = errno; goto error;}
    /* Receive the HTTP request from the client. */
    char command[32];
    int rc = dill_http_recvrequest(s, command, sizeof(command),
        resource, resourcelen, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    if(dill_slow(strcmp(command, "GET") != 0)) {err = EPROTO; goto error;}
    int has_host = 0;
    int has_upgrade = 0;
    int has_connection = 0;
    int has_key = 0;
    int has_version = 0;
    char response_key[WS_KEY_SIZE];
    while(1) {
        char name[256];
        char value[256];
        rc = dill_http_recvfield(s, name, sizeof(name), value, sizeof(value),
            deadline);
        if(rc < 0 && errno == EPIPE) break;
        if(dill_slow(rc < 0)) {err = errno; goto error;}
        if(strcasecmp(name, "Host") == 0) {
           if(has_host != 0) {err = EPROTO; goto error;}
           /* TODO: Is this the correct error code? */
           if(dill_slow(strlen(value) >= hostlen)) {
               err = EMSGSIZE; goto error;}
           strcpy(host, value);
           has_host = 1;
           continue;
        }
        if(strcasecmp(name, "Upgrade") == 0) {
           if(has_upgrade || strcasecmp(value, "websocket") != 0) {
               err = EPROTO; goto error;}
           has_upgrade = 1;
           continue;
        }
        if(strcasecmp(name, "Connection") == 0) {
           if(has_connection || strcasecmp(value, "Upgrade") != 0) {
               err = EPROTO; goto error;}
           has_connection = 1;
           continue;
        }
        if(strcasecmp(name, "Sec-WebSocket-Key") == 0) {
            if(has_key) {err = EPROTO; goto error;}
            /* Generate the key to be sent back to the client. */
            rc = dill_ws_response_key(value, response_key);
            if(dill_slow(rc < 0)) {err = errno; goto error;}
            has_key = 1;
            continue;
        }
        if(strcasecmp(name, "Sec-WebSocket-Version") == 0) {
           if(has_version || strcasecmp(value, "13") != 0) {
               err = EPROTO; goto error;}
           has_version = 1;
           continue;
        }
    }
    if(dill_slow(!has_upgrade || !has_connection || !has_key || !has_version)) {
        err = EPROTO; goto error;}

    /* Send HTTP response back to the client. */
    rc = dill_http_sendstatus(s, 101, "Switching Protocols", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_http_sendfield(s, "Upgrade", "websocket", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_http_sendfield(s, "Connection", "Upgrade", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_http_sendfield(s, "Sec-WebSocket-Accept", response_key, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}

    s = dill_http_detach(s, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error;}
    self->u = s;
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error;}
    return h;
error:
    if(s >= 0) {
        int rc = dill_hclose(s);
        dill_assert(rc == 0);
    }
    errno = err;
    return -1;
}

int dill_ws_attach_server(int s, int flags, char *resource, size_t resourcelen,
      char *host, size_t hostlen, int64_t deadline) {
    int err;
    struct dill_ws_sock *obj = malloc(sizeof(struct dill_ws_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    s = dill_ws_attach_server_mem(s, flags, resource, resourcelen,
        host, hostlen, (struct dill_ws_storage*)obj, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

static int dill_ws_sendl_base(struct dill_msock_vfs *mvfs, uint8_t type,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_ws_sock *self = dill_cont(mvfs, struct dill_ws_sock, mvfs);
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    size_t len;
    int rc = dill_iolcheck(first, last, NULL, &len);
    if(dill_slow(rc < 0)) return -1;
    uint8_t buf[12];
    size_t sz;
    buf[0] = 0x80 | type;
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
    rc = dill_bsend(self->u, buf, sz, deadline);
    if(dill_slow(rc < 0)) return -1;
    /* On the server side we can send the payload as is. */
    if(self->server) {
        rc = dill_bsendl(self->u, first, last, deadline);
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
            rc = dill_bsend(self->u, &b, 1, deadline); 
            if(dill_slow(rc < 0)) return -1;
        }
        first = first->iol_next;
    }
    return 0;
}

static ssize_t dill_ws_recvl_base(struct dill_msock_vfs *mvfs, int *flags,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_ws_sock *self = dill_cont(mvfs, struct dill_ws_sock, mvfs);
    if(dill_slow(self->indone)) {errno = EPIPE; return -1;}
    int rc = dill_iolcheck(first, last, NULL, NULL);
    if(dill_slow(rc < 0)) return -1;
    size_t res = 0;
    /* Message may consist of multiple frames. Read them one by one. */
    struct dill_iolist it;
    if(first) it = *first;
    while(1) {
        struct dill_iolist iol_msg;
        uint8_t hdr1[2];
        rc = dill_brecv(self->u, hdr1, sizeof(hdr1), deadline);
        if(dill_slow(rc < 0)) return -1;
        if(hdr1[0] & 0x70) {errno = EPROTO; return -1;}
        int opcode = hdr1[0] & 0x0f;
        /* TODO: Other opcodes. */
        switch(opcode) {
        case 0:
            break;
        case 1:
            if(flags) *flags = DILL_WS_TEXT;
            break;
        case 2:
            if(flags) *flags = DILL_WS_BINARY;
            break;
        case 8:
            it.iol_base = &self->status;
            it.iol_len = 2;
            it.iol_next = &iol_msg;
            it.iol_rsvd = 0;
            iol_msg.iol_base = self->msg;
            iol_msg.iol_len = sizeof(self->msg);
            iol_msg.iol_next = NULL;
            iol_msg.iol_rsvd = 0;
            self->indone = 1;
            break;
        default:
            errno = EPROTO;
            return -1;
        }
        if(!self->server ^ !(hdr1[1] & 0x80)) {errno = EPROTO; return -1;}
        size_t sz = hdr1[1] & 0x7f;
        if(sz == 126) {
            uint8_t hdr2[2];
            rc = dill_brecv(self->u, hdr2, sizeof(hdr2), deadline);
            if(dill_slow(rc < 0)) return -1;
            sz = dill_gets(hdr2);
        }
        else if(sz == 127) {
            uint8_t hdr2[8];
            rc = dill_brecv(self->u, hdr2, sizeof(hdr2), deadline);
            if(dill_slow(rc < 0)) return -1;
            sz = dill_getll(hdr2);
        }
        res += sz;
        /* Control frames cannot be fragmented or longer than 125 bytes. */
        if(dill_slow(opcode >= 8 && (sz > 125 || !(hdr1[0] & 0x80)))) {
            errno = EPROTO; return -1;}
        uint8_t mask[4];
        if(self->server) {
            rc = dill_brecv(self->u, mask, sizeof(mask), deadline);
            if(dill_slow(rc < 0)) return -1;
        }
        /* Frame may be read into multiple iolist elements. */
        while(1) {
            size_t toread = sz < it.iol_len ? sz : it.iol_len;
            if(toread > 0) {
                rc = dill_brecv(self->u, it.iol_base, toread, deadline);
                if(dill_slow(rc < 0)) return -1;
                if(self->server) {
                    size_t i;
                    for(i = 0; i != toread; ++i)
                        ((uint8_t*)it.iol_base)[i] ^= mask[i % 4];
                }
            }
            sz -= toread;
            if(sz == 0) break;
            if(dill_slow(!it.iol_next)) {errno = EMSGSIZE; return -1;}
            it = *it.iol_next;
        }
        if(hdr1[0] & 0x80) break;
    }
    if(dill_slow(self->indone)) {
        if(dill_slow(res == 1)) {errno = EPROTO; return -1;}
        if(res >= 2) {
           self->status = dill_gets((uint8_t*)&self->status);
           if(dill_slow(self->status < 1000 || self->status > 4999)) {
               errno = EPROTO; return -1;}
           self->msglen = (uint8_t)res - 2;
        }
        else self->msglen = 0;
        errno = EPIPE;
        return -1;
    }
    return res;
}

int dill_ws_send(int s, int flags, const void *buf, size_t len,
      int64_t deadline) {
    struct dill_ws_sock *self = dill_hquery(s, dill_ws_type);
    if(dill_slow(!self)) return -1;
    struct dill_iolist iol = {(void*)buf, len, NULL, 0};
    return dill_ws_sendl_base(&self->mvfs, (flags & DILL_WS_TEXT) ? 0x1 : 0x2,
        &iol, &iol, deadline);
}

ssize_t dill_ws_recv(int s, int *flags, void *buf, size_t len,
      int64_t deadline) {
    struct dill_ws_sock *self = dill_hquery(s, dill_ws_type);
    if(dill_slow(!self)) return -1;
    struct dill_iolist iol = {(void*)buf, len, NULL, 0};
    return dill_ws_recvl_base(&self->mvfs, flags, &iol, &iol, deadline);
}

int dill_ws_sendl(int s, int flags, struct dill_iolist *first,
      struct dill_iolist *last, int64_t deadline) {
    struct dill_ws_sock *self = dill_hquery(s, dill_ws_type);
    if(dill_slow(!self)) return -1;
    return dill_ws_sendl_base(&self->mvfs, (flags & DILL_WS_TEXT) ? 0x1 : 0x2,
        first, last, deadline);
}

ssize_t dill_ws_recvl(int s, int *flags, struct dill_iolist *first,
      struct dill_iolist *last, int64_t deadline) {
    struct dill_ws_sock *self = dill_hquery(s, dill_ws_type);
    if(dill_slow(!self)) return -1;
    return dill_ws_recvl_base(&self->mvfs, flags, first, last, deadline);
}

static int dill_ws_msendl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_ws_sock *self = dill_cont(mvfs, struct dill_ws_sock, mvfs);
    return dill_ws_sendl_base(mvfs, (self->flags & DILL_WS_TEXT) ? 0x1 : 0x2,
        first, last, deadline);
}

static ssize_t dill_ws_mrecvl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_ws_sock *self = dill_cont(mvfs, struct dill_ws_sock, mvfs);
    int flags;
    ssize_t sz = dill_ws_recvl_base(mvfs, &flags, first, last, deadline);
    if(dill_slow(sz < 0)) return -1;
    if(dill_slow((flags & DILL_WS_TEXT) != (self->flags & DILL_WS_TEXT))) {
        errno = EPROTO; return -1;}
    return sz;
}

int dill_ws_done(int s, int status, const void *buf, size_t len,
      int64_t deadline) {
    if(dill_slow((status != 0 && (status < 1000 || status > 4999)) ||
        (!buf && len > 0))) {errno = EINVAL; return -1;}
    struct dill_ws_sock *self = dill_hquery(s, dill_ws_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    struct dill_iolist iol2 = {(void*)buf, len, NULL, 0};
    uint8_t sbuf[2];
    dill_puts(sbuf, status);
    struct dill_iolist iol1 = {status ? sbuf : NULL, status ? sizeof(sbuf) : 0,
        &iol2, 0};
    int rc = dill_ws_sendl_base(&self->mvfs, 0x8, &iol1, &iol2, deadline);
    if(dill_slow(rc < 0)) return -1;
    self->outdone = 1;
    return 0;
}

int dill_ws_detach(int s, int status, const void *buf, size_t len,
      int64_t deadline) {
    struct dill_ws_sock *self = dill_hquery(s, dill_ws_type);
    if(dill_slow(!self)) return -1;
    if(!self->outdone) {
        int rc = dill_ws_done(s, status, buf, len, deadline);
        if(dill_slow(rc < 0)) return -1;
    }
    while(1) {
        struct dill_iolist iol = {NULL, SIZE_MAX, NULL, 0};
        ssize_t sz = dill_ws_recvl_base(&self->mvfs, NULL, &iol, &iol,
            deadline);
        if(sz < 0) {
             if(errno == EPIPE) break;
             return -1;
        }
    }
    int u = self->u;
    if(!self->mem) free(self);
    return u;
}

static void dill_ws_hclose(struct dill_hvfs *hvfs) {
    struct dill_ws_sock *self = (struct dill_ws_sock*)hvfs;
    int rc = dill_hclose(self->u);
    dill_assert(rc == 0);
    if(!self->mem) free(self);
}

ssize_t dill_ws_status(int s, int *status, void *buf, size_t len) {
    if(dill_slow(!buf && len)) {errno = EINVAL; return -1;}
    struct dill_ws_sock *self = dill_hquery(s, dill_ws_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(!self->indone)) {errno = EAGAIN; return -1;}
    if(status) *status = self->status;
    if(buf) {
        if(dill_slow(len < self->msglen)) {errno = EMSGSIZE; return -1;}
        memcpy(buf, self->msg, self->msglen);
    }
    return self->msglen;
}

/******************************************************************************/
/*  Helper functions.                                                         */
/******************************************************************************/

int dill_ws_request_key(char *request_key) {
    if(dill_slow(!request_key)) {errno = EINVAL; return -1;}
    uint8_t nonce[16];
    int rc = dill_random(nonce, sizeof(nonce));
    if(dill_slow(rc < 0)) return -1;
    rc = dill_base64_encode(nonce, sizeof(nonce), request_key, WS_KEY_SIZE);
    if(dill_slow(rc < 0)) {errno = EFAULT; return -1;}
    return 0;
}

int dill_ws_response_key(const char *request_key, char *response_key) {
    if(dill_slow(!request_key)) {errno = EINVAL; return -1;}
    if(dill_slow(!response_key)) {errno = EINVAL; return -1;}
    /* Decode the request key and check whether it's a 16-byte nonce. */
    uint8_t nonce[16];
    int rc = dill_base64_decode(request_key, strlen(request_key),
        nonce, sizeof(nonce));
    if(dill_slow(rc != sizeof(nonce))) {errno = EPROTO; return -1;}
    /* Create the response key. */
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
    rc = dill_base64_encode(dill_sha1_result(&sha1), DILL_SHA1_HASH_LEN,
        response_key, WS_KEY_SIZE);
    if(dill_slow(rc < 0)) {errno = EFAULT; return -1;}
    return 0;
}

