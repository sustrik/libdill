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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "utils.h"

const struct dill_http_opts dill_http_defaults = {
    NULL  /* mem */
};

dill_unique_id(dill_http_type);

static void *dill_http_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_http_hclose(struct dill_hvfs *hvfs);

struct dill_http_sock {
    struct dill_hvfs hvfs;
    /* Underlying SUFFIX socket. */
    int u;
    unsigned int mem : 1;
    struct dill_suffix_storage suffix_mem;
    struct dill_term_storage term_mem;
    char rxbuf[1024];
};

DILL_CHECK_STORAGE(dill_http_sock, dill_http_storage)

static void *dill_http_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_http_sock *self = (struct dill_http_sock*)hvfs;
    if(type == dill_http_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int dill_http_attach(int s, const struct dill_http_opts *opts) {
    int err;
    if(!opts) opts = &dill_http_defaults;
    /* Check whether underlying socket is a bytestream. */
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) {err = errno; goto error1;}
    /* Create the object. */
    struct dill_http_sock *self = (struct dill_http_sock*)opts->mem;
    if(!self) {
        self = malloc(sizeof(struct dill_http_sock));
        if(dill_slow(!self)) {err = ENOMEM; goto error1;}
    }
    /* Wrap the underlying socket into SUFFIX and TERM protocol. */
    struct dill_suffix_opts sopts = dill_suffix_defaults;
    sopts.mem = &self->suffix_mem;
    int rc = dill_suffix_attach(s, "\r\n", 2, &sopts);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    struct dill_term_opts topts = dill_term_defaults;
    topts.mem = &self->term_mem;
    rc = dill_term_attach(s, NULL, 0, &topts);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    self->hvfs.query = dill_http_hquery;
    self->hvfs.close = dill_http_hclose;
    self->mem = !!opts->mem;
    self->u = dill_hattach(s, &self->hvfs);
    if(dill_slow(self->u < 0)) {err = errno; goto error2;}
    return 0;
error2:
    if(!opts->mem) free(self);
error1:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_http_done(int s, int64_t deadline) {
    struct dill_http_sock *self = dill_hquery(s, dill_http_type);
    if(dill_slow(!self)) return -1;
    return dill_term_done(self->u, deadline);
}

int dill_http_detach(int s, int64_t deadline) {
    int err;
    struct dill_http_sock *self = dill_hquery(s, dill_http_type);
    if(dill_slow(!self)) return -1;
    int rc = dill_term_detach(self->u, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_suffix_detach(self->u);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    rc = dill_hdetach(s, self->u);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    if(!self->mem) free(self);
    return 0;
error:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_http_sendrequest(int s, const char *command, const char *resource,
      int64_t deadline) {
    struct dill_http_sock *self = dill_hquery(s, dill_http_type);
    if(dill_slow(!self)) return -1;
    if (strpbrk(command, " \t\n") != NULL) {errno = EINVAL; return -1;}
    if (strpbrk(resource, " \t\n") != NULL) {errno = EINVAL; return -1;}
    struct dill_iolist iol[4];
    iol[0].iol_base = (void*)command;
    iol[0].iol_len = strlen(command);
    iol[0].iol_next = &iol[1];
    iol[0].iol_rsvd = 0;
    iol[1].iol_base = (void*)" ";
    iol[1].iol_len = 1;
    iol[1].iol_next = &iol[2];
    iol[1].iol_rsvd = 0;
    iol[2].iol_base = (void*)resource;
    iol[2].iol_len = strlen(resource);
    iol[2].iol_next = &iol[3];
    iol[2].iol_rsvd = 0;
    iol[3].iol_base = (void*)" HTTP/1.1";
    iol[3].iol_len = 9;
    iol[3].iol_next = NULL;
    iol[3].iol_rsvd = 0;
    return dill_msendl(self->u, &iol[0], &iol[3], deadline);
}

int dill_http_recvrequest(int s, char *command, size_t commandlen,
      char *resource, size_t resourcelen, int64_t deadline) {
    struct dill_http_sock *self = dill_hquery(s, dill_http_type);
    if(dill_slow(!self)) return -1;
    ssize_t sz = dill_mrecv(self->u, self->rxbuf, sizeof(self->rxbuf) - 1,
        deadline);
    if(dill_slow(sz < 0)) return -1;
    self->rxbuf[sz] = 0;
    size_t pos = 0;
    while(self->rxbuf[pos] == ' ') ++pos;
    /* Command. */
    size_t start = pos;
    while(self->rxbuf[pos] != 0 && self->rxbuf[pos] != ' ') ++pos;
    if(dill_slow(self->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dill_slow(pos - start > commandlen - 1)) {errno = EMSGSIZE; return -1;}
    memcpy(command, self->rxbuf + start, pos - start);
    command[pos - start] = 0;
    while(self->rxbuf[pos] == ' ') ++pos;
    /* Resource. */
    start = pos;
    while(self->rxbuf[pos] != 0 && self->rxbuf[pos] != ' ') ++pos;
    if(dill_slow(self->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dill_slow(pos - start > resourcelen - 1)) {errno = EMSGSIZE; return -1;}
    memcpy(resource, self->rxbuf + start, pos - start);
    resource[pos - start] = 0;
    while(self->rxbuf[pos] == ' ') ++pos;
    /* Protocol. */
    start = pos;
    while(self->rxbuf[pos] != 0 && self->rxbuf[pos] != ' ') ++pos;
    if(dill_slow(pos - start != 8 &&
          memcmp(self->rxbuf + start, "HTTP/1.1", 8) != 0)) {
        errno = EPROTO; return -1;}
    while(self->rxbuf[pos] == ' ') ++pos;
    if(dill_slow(self->rxbuf[pos] != 0)) {errno = EPROTO; return -1;}
    return 0;
}

int dill_http_sendstatus(int s, int status, const char *reason,
      int64_t deadline) {
    struct dill_http_sock *self = dill_hquery(s, dill_http_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(status < 100 || status > 599)) {errno = EINVAL; return -1;}
    char buf[4];
    buf[0] = (status / 100) + '0';
    status %= 100;
    buf[1] = (status / 10) + '0';
    status %= 10;
    buf[2] = status + '0';
    buf[3] = ' ';
    struct dill_iolist iol[3];
    iol[0].iol_base = (void*)"HTTP/1.1 ";
    iol[0].iol_len = 9;
    iol[0].iol_next = &iol[1];
    iol[0].iol_rsvd = 0;
    iol[1].iol_base = buf;
    iol[1].iol_len = 4;
    iol[1].iol_next = &iol[2];
    iol[1].iol_rsvd = 0;
    iol[2].iol_base = (void*)reason;
    iol[2].iol_len = strlen(reason);
    iol[2].iol_next = NULL;
    iol[2].iol_rsvd = 0;
    return dill_msendl(self->u, &iol[0], &iol[2], deadline);
}

int dill_http_recvstatus(int s, char *reason, size_t reasonlen,
      int64_t deadline) {
    struct dill_http_sock *self = dill_hquery(s, dill_http_type);
    if(dill_slow(!self)) return -1;
    ssize_t sz = dill_mrecv(self->u, self->rxbuf, sizeof(self->rxbuf) - 1,
        deadline);
    if(dill_slow(sz < 0)) return -1;
    self->rxbuf[sz] = 0;
    size_t pos = 0;
    while(self->rxbuf[pos] == ' ') ++pos;
    /* Protocol. */
    size_t start = pos;
    while(self->rxbuf[pos] != 0 && self->rxbuf[pos] != ' ') ++pos;
    if(dill_slow(self->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dill_slow(pos - start != 8 &&
          memcmp(self->rxbuf + start, "HTTP/1.1", 8) != 0)) {
        errno = EPROTO; return -1;}
    while(self->rxbuf[pos] == ' ') ++pos;
    /* Status code. */
    start = pos;
    while(self->rxbuf[pos] != 0 && self->rxbuf[pos] != ' ') ++pos;
    if(dill_slow(self->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dill_slow(pos - start != 3)) {errno = EPROTO; return -1;}
    if(dill_slow(self->rxbuf[start] < '0' || self->rxbuf[start] > '9' ||
          self->rxbuf[start + 1] < '0' || self->rxbuf[start + 1] > '9' ||
          self->rxbuf[start + 2] < '0' || self->rxbuf[start + 2] > '9')) {
        errno = EPROTO; return -1;}
    int status = (self->rxbuf[start] - '0') * 100 +
        (self->rxbuf[start + 1] - '0') * 10 +
        (self->rxbuf[start + 2] - '0');
    while(self->rxbuf[pos] == ' ') ++pos;
    /* Reason. */
    if(sz - pos > reasonlen - 1) {errno = EMSGSIZE; return -1;}
    memcpy(reason, self->rxbuf + pos, sz - pos);
    reason[sz - pos] = 0;
    return status;
}

int dill_http_sendfield(int s, const char *name, const char *value,
      int64_t deadline) {
    struct dill_http_sock *self = dill_hquery(s, dill_http_type);
    if(dill_slow(!self)) return -1;
    /* Check whether name contains only valid characters. */
    if (strpbrk(name, "(),/:;<=>?@[\\]{}\" \t\n") != NULL) {
        errno = EINVAL; return -1;}
    if (strlen(value) == 0) {errno = EPROTO; return -1;}
    struct dill_iolist iol[3];
    iol[0].iol_base = (void*)name;
    iol[0].iol_len = strlen(name);
    iol[0].iol_next = &iol[1];
    iol[0].iol_rsvd = 0;
    iol[1].iol_base = (void*)": ";
    iol[1].iol_len = 2;
    iol[1].iol_next = &iol[2];
    iol[1].iol_rsvd = 0;
    const char *start = dill_lstrip(value, ' ');
    const char *end = dill_rstrip(start, ' ');
    dill_assert(start < end);
    iol[2].iol_base = (void*)start;
    iol[2].iol_len = end - start;
    iol[2].iol_next = NULL;
    iol[2].iol_rsvd = 0;
    return dill_msendl(self->u, &iol[0], &iol[2], deadline);
}

int dill_http_recvfield(int s, char *name, size_t namelen,
      char *value, size_t valuelen, int64_t deadline) {
    struct dill_http_sock *self = dill_hquery(s, dill_http_type);
    if(dill_slow(!self)) return -1;
    ssize_t sz = dill_mrecv(self->u, self->rxbuf, sizeof(self->rxbuf) - 1,
        deadline);
    if(dill_slow(sz < 0)) return -1;
    self->rxbuf[sz] = 0;
    size_t pos = 0;
    while(self->rxbuf[pos] == ' ') ++pos;
    /* Name. */
    size_t start = pos;
    while(self->rxbuf[pos] != 0 &&
          self->rxbuf[pos] != ' ' &&
          self->rxbuf[pos] != ':')
        ++pos;
    if(dill_slow(self->rxbuf[pos] == 0)) {errno = EPROTO; return -1;}
    if(dill_slow(pos - start > namelen - 1)) {errno = EMSGSIZE; return -1;}
    memcpy(name, self->rxbuf + start, pos - start);
    name[pos - start] = 0;
    while(self->rxbuf[pos] == ' ') ++pos;
    if(dill_slow(self->rxbuf[pos] != ':')) {errno = EPROTO; return -1;}
    ++pos;
    while(self->rxbuf[pos] == ' ') ++pos;
    /* Value. */
    start = pos;
    pos = dill_rstrip(self->rxbuf + sz, ' ') - self->rxbuf;
    if(dill_slow(pos - start > valuelen - 1)) {errno = EMSGSIZE; return -1;}
    memcpy(value, self->rxbuf + start, pos - start);
    value[pos - start] = 0;
    while(self->rxbuf[pos] == ' ') ++pos;
    if(dill_slow(self->rxbuf[pos] != 0)) {errno = EPROTO; return -1;}
    return 0;
}

static void dill_http_hclose(struct dill_hvfs *hvfs) {
    struct dill_http_sock *self = (struct dill_http_sock*)hvfs;
    if(dill_fast(self->u >= 0)) {
        int rc = dill_hclose(self->u);
        dill_assert(rc == 0);
    }
    if(!self->mem) free(self);
}

