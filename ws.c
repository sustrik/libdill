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

#include "utils.h"

dill_unique_id(ws_type);

struct ws_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    char wsraw[WSRAW_SIZE];
    unsigned int mem : 1;
};

DILL_CT_ASSERT(WS_SIZE >= sizeof(struct ws_sock));

static void *ws_hquery(struct hvfs *hvfs, const void *type);
static void ws_hclose(struct hvfs *hvfs);
static int ws_hdone(struct hvfs *hvfs, int64_t deadline);
static int ws_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t ws_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

static void *ws_hquery(struct hvfs *hvfs, const void *type) {
    struct ws_sock *self = (struct ws_sock*)hvfs;
    if(type == msock_type) return &self->mvfs;
    if(type == ws_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int ws_attach_client_mem(int s, const char *resource, const char *host,
      int type, void *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    if(dill_slow(type != WS_TYPE_BINARY && type != WS_TYPE_TEXT)) {
        errno = EINVAL; goto error1;}
    char http_mem[HTTP_SIZE];
    int http = http_attach_mem(s, http_mem);
    if(dill_slow(http < 0)) {err = errno; goto error1;}
    /* Initial request. */
    int rc = http_sendrequest(http, "GET", resource, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    rc = http_sendfield(http, "Host", host, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    rc = http_sendfield(http, "Upgrade", "websocket", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    rc = http_sendfield(http, "Connection", "Upgrade", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    char request_key[WSRAW_KEY_SIZE];
    rc = wsraw_request_key(request_key);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    rc = http_sendfield(http, "Sec-WebSocket-Key", request_key, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    rc = http_sendfield(http, "Sec-WebSocket-Version", "13", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    /* TODO: Protocol, Extensions, custom fields? */
    rc = hdone(http, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    /* Receive the reply from the server. */
    char reason[256];
    rc = http_recvstatus(http, reason, sizeof(reason), deadline);
    if(dill_slow(rc != 101)) {err = EPROTO; goto error1;}
    int has_upgrade = 0;
    int has_connection = 0;
    int has_key = 0;
    while(1) {
        char name[256];
        char value[256];
        rc = http_recvfield(http, name, sizeof(name), value, sizeof(value), -1);
        if(rc < 0 && errno == EPIPE) break;
        if(dill_slow(rc < 0)) {err = errno; goto error1;}
        if(strcasecmp(name, "Upgrade") == 0) {
           if(has_upgrade || strcasecmp(value, "websocket") != 0) {
               err = EPROTO; goto error1;}
           has_upgrade = 1;
           continue;
        }
        if(strcasecmp(name, "Connection") == 0) {
           if(has_connection || strcasecmp(value, "Upgrade") != 0) {
               err = EPROTO; goto error1;}
           has_connection = 1;
           continue;
        }
        if(strcasecmp(name, "Sec-WebSocket-Accept") == 0) {
            if(has_key) {err = EPROTO; goto error1;}
            char response_key[WSRAW_KEY_SIZE];
            rc = wsraw_response_key(request_key, response_key);
            if(dill_slow(rc < 0)) {err = errno; goto error1;}
            if(dill_slow(strcmp(value, response_key) != 0)) {
                err = EPROTO; goto error1;}
            has_key = 1;
            continue;
        }
    }
    if(!has_upgrade) {err = EPROTO; goto error1;}
    if(!has_connection) {err = EPROTO; goto error1;}
    if(!has_key) {err = EPROTO; goto error1;}
    s = http_detach(http, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    struct ws_sock *self = (struct ws_sock*)mem;
    s = wsraw_attach_client_mem(s, type, self->wsraw, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Create the object. */
    self->hvfs.query = ws_hquery;
    self->hvfs.close = ws_hclose;
    self->hvfs.done = ws_hdone;
    self->mvfs.msendl = ws_msendl;
    self->mvfs.mrecvl = ws_mrecvl;
    self->u = s;
    self->mem = 1;
    /* Create the handle. */
    int h = hmake(&self->hvfs);
    if(dill_slow(h < 0)) {int err = errno; goto error1;}
    return h;
error1:
    perror("error");
    dill_assert(0);
}

int ws_attach_client(int s, const char *resource, const char *host,
      int type, int64_t deadline) {
    int err;
    struct ws_sock *obj = malloc(sizeof(struct ws_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int ws = ws_attach_client_mem(s, resource, host, type, obj, deadline);
    if(dill_slow(ws < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return ws;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int ws_attach_server_mem(int s, int type, void *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    if(dill_slow(type != WS_TYPE_BINARY && type != WS_TYPE_TEXT)) {
        errno = EINVAL; goto error1;}
    char http_mem[HTTP_SIZE];
    int http = http_attach_mem(s, http_mem);
    if(dill_slow(http < 0)) {err = errno; goto error1;}
    /* Receive the request from the client. */
    char command[32];
    char resource[256];
    int rc = http_recvrequest(http, command, sizeof(command),
        resource, sizeof(resource), deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    if(dill_slow(strcmp(command, "GET") != 0)) {err = EPROTO; goto error1;}
    int has_host = 0;
    int has_upgrade = 0;
    int has_connection = 0;
    int has_key = 0;
    int has_version = 0;
    char response_key[WSRAW_KEY_SIZE];
    while(1) {
        char name[256];
        char value[256];
        rc = http_recvfield(http, name, sizeof(name), value, sizeof(value), -1);
        if(rc < 0 && errno == EPIPE) break;
        if(dill_slow(rc < 0)) {err = errno; goto error1;}
        if(strcasecmp(name, "Host") == 0) {
           if(has_host != 0) {err = EPROTO; goto error1;}
           has_host = 1;
           continue;
        }
        if(strcasecmp(name, "Upgrade") == 0) {
           if(has_upgrade || strcasecmp(value, "websocket") != 0) {
               err = EPROTO; goto error1;}
           has_upgrade = 1;
           continue;
        }
        if(strcasecmp(name, "Connection") == 0) {
           if(has_connection || strcasecmp(value, "Upgrade") != 0) {
               err = EPROTO; goto error1;}
           has_connection = 1;
           continue;
        }
        if(strcasecmp(name, "Sec-WebSocket-Key") == 0) {
            if(has_key) {err = EPROTO; goto error1;}
            /* Decode the key and check whether it's 16 byte nonce. */
            uint8_t nonce[16];
            rc = dill_base64_decode(value, strlen(value), nonce, sizeof(nonce));
            if(dill_slow(rc < 0)) {err = EPROTO; goto error1;}
            /* Generate the key to be sent back to the client. */
            rc = wsraw_response_key(value, response_key);
            if(dill_slow(rc < 0)) {err = EFAULT; goto error1;}
            has_key = 1;
            continue;
        }
        if(strcasecmp(name, "Sec-WebSocket-Version") == 0) {
           if(has_version || strcasecmp(value, "13") != 0) {
               err = EPROTO; goto error1;}
           has_version = 1;
           continue;
        }
    }
    if(!has_upgrade) {err = EPROTO; goto error1;}
    if(!has_connection) {err = EPROTO; goto error1;}
    if(!has_key) {err = EPROTO; goto error1;}
    if(!has_version) {err = EPROTO; goto error1;}
    /* Send response back to the client. */
    rc = http_sendstatus(http, 101, "Switching Protocols", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    rc = http_sendfield(http, "Upgrade", "websocket", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    rc = http_sendfield(http, "Connection", "Upgrade", deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    rc = http_sendfield(http, "Sec-WebSocket-Accept", response_key, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    s = http_detach(http, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    struct ws_sock *self = (struct ws_sock*)mem;
    s = wsraw_attach_server_mem(s, type, self->wsraw, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Create the object. */
    self->hvfs.query = ws_hquery;
    self->hvfs.close = ws_hclose;
    self->hvfs.done = ws_hdone;
    self->mvfs.msendl = ws_msendl;
    self->mvfs.mrecvl = ws_mrecvl;
    self->u = s;
    self->mem = 1;
    /* Create the handle. */
    int h = hmake(&self->hvfs);
    if(dill_slow(h < 0)) {int err = errno; goto error1;}
    return h;
error1:
    perror("error");
    dill_assert(0);
}

int ws_attach_server(int s, int type, int64_t deadline) {
    int err;
    struct ws_sock *obj = malloc(sizeof(struct ws_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int ws = ws_attach_server_mem(s, type, obj, deadline);
    if(dill_slow(ws < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return ws;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int ws_send(int s, int type, const void *buf, size_t len, int64_t deadline) {
    struct iolist iol = {(void*)buf, len, NULL, 0};
    return ws_sendl(s, type, &iol, &iol, deadline);
}

ssize_t ws_recv(int s, int *type, void *buf, size_t len, int64_t deadline) {
    struct iolist iol = {(void*)buf, len, NULL, 0};
    return ws_recvl(s, type, &iol, &iol, deadline);
}

int ws_sendl(int s, int type, struct iolist *first, struct iolist *last,
      int64_t deadline) {
    struct ws_sock *self = hquery(s, ws_type);
    if(dill_slow(!self)) return -1;
    return wsraw_sendl(self->u, type, first, last, deadline);
}

ssize_t ws_recvl(int s, int *type, struct iolist *first, struct iolist *last,
      int64_t deadline) {
    struct ws_sock *self = hquery(s, ws_type);
    if(dill_slow(!self)) return -1;
    return wsraw_recvl(self->u, type, first, last, deadline);
}

static int ws_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct ws_sock *self = dill_cont(mvfs, struct ws_sock, mvfs);
    return msendl(self->u, first, last, deadline);
}

static ssize_t ws_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct ws_sock *self = dill_cont(mvfs, struct ws_sock, mvfs);
    return mrecvl(self->u, first, last, deadline);
}

static int ws_hdone(struct hvfs *hvfs, int64_t deadline) {
    dill_assert(0);
}

static void ws_hclose(struct hvfs *hvfs) {
    dill_assert(0);
}

