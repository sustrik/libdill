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
#include <stdlib.h>
#include <string.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "utils.h"

const struct dill_tcpmux_opts dill_tcpmux_defaults = {
    NULL,           /* mem */
    "/tmp/tcpmux",  /* addr */
};

dill_unique_id(dill_tcpmux_type);

static void *dill_tcpmux_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_tcpmux_hclose(struct dill_hvfs *hvfs);

struct dill_tcpmux_sock {
    struct dill_hvfs hvfs;
    int s;
    struct dill_ipc_storage ipcmem;
    unsigned int mem : 1;
};

DILL_CHECK_STORAGE(dill_tcpmux_sock, dill_tcpmux_storage)

static void *dill_tcpmux_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_tcpmux_sock *obj = (struct dill_tcpmux_sock*)hvfs;
    if(type == dill_tcpmux_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

static void dill_tcpmux_hclose(struct dill_hvfs *hvfs) {
    struct dill_tcpmux_sock *self = (struct dill_tcpmux_sock*)hvfs;
    if(dill_fast(self->s >= 0)) {
        int rc = dill_hclose(self->s);
        dill_assert(rc == 0);
    }
    if(!self->mem) free(self);
}

int dill_tcpmux_listen(const char *service,
      const struct dill_tcpmux_opts *opts, int64_t deadline) {
    int err;
    if(dill_slow(!service)) {err = EINVAL; goto error1;}
    size_t servicelen = strlen(service);
    if(dill_slow(servicelen > 255)) {err = EINVAL; goto error1;}
    if(!opts) opts = &dill_tcpmux_defaults;
    /* Create the object. */
    struct dill_tcpmux_sock *self = (struct dill_tcpmux_sock*)opts->mem;
    if(!self) {
        self = malloc(sizeof(struct dill_tcpmux_sock));
        if(dill_slow(!self)) {err = ENOMEM; goto error1;}
    }
    self->hvfs.query = dill_tcpmux_hquery;
    self->hvfs.close = dill_tcpmux_hclose;
    self->mem = !!opts->mem;
    /* Register the service with tcpmuxd. */
    struct dill_ipc_opts ipcopts = dill_ipc_defaults;
    ipcopts.rx_buffering = 0;
    ipcopts.mem = &self->ipcmem;
    self->s = dill_ipc_connect(opts->addr, &ipcopts, deadline);
    if(dill_slow(self->s < 0)) {err = errno; goto error2;}
    uint8_t val = (uint8_t)servicelen;
    int rc = dill_bsend(self->s, &val, 1, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error3;}
    rc = dill_bsend(self->s, service, servicelen, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error3;}
    rc = dill_brecv(self->s, &val, 1, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error3;}
    if(dill_slow(val == 0)) {err = EPROTO; goto error3;} /* TODO: error code? */
    /* Create the handle. */
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    rc = dill_hclose(self->s);
    dill_assert(rc == 0);
error2:
    if(!self->mem) free(self);
error1:
    errno = err;
    return -1;
}

int dill_tcpmux_accept(int s, const struct dill_tcp_opts *opts,
      struct dill_ipaddr *addr, int64_t deadline) {
    struct dill_tcpmux_sock *self = dill_hquery(s, dill_tcpmux_type);
    if(dill_slow(!self)) return -1;
    if(!opts) opts = &dill_tcp_defaults;
    int as = dill_ipc_recvfd(self->s, deadline);
    dill_assert(as >= 0);
    as = dill_tcp_attach(as, opts);
    dill_assert(as >= 0);
    return as;
}

/* TODO: How to combine this with happy eyeballs? */
int dill_tcpmux_connect(const struct dill_ipaddr *addr, const char *service,
      const struct dill_tcp_opts *opts, int64_t deadline) {
    if(dill_slow(!service)) {errno = EINVAL; return -1;}
    size_t servicelen = strlen(service);
    if(dill_slow(servicelen > 255)) {errno = EINVAL; return -1;}
    if(!opts) opts = &dill_tcp_defaults;
    int s = dill_tcp_connect(addr, opts, deadline);
    if(dill_slow(s < 0)) return -1;
    struct dill_suffix_storage sfxmem;
    struct dill_suffix_opts sfxopts = dill_suffix_defaults;
    sfxopts.mem = &sfxmem;
    s = dill_suffix_attach(s, "\r\n", 2, &sfxopts);
    dill_assert(s >= 0);
    int rc = dill_msend(s, service, servicelen, deadline);
    dill_assert(rc == 0);
    char buf[1]; 
    memset(buf, 0, sizeof(buf));
    ssize_t sz = dill_mrecv(s, buf, 1, deadline);
    dill_assert(sz >= 0);
    dill_assert(sz != 0); // EPROTO
    dill_assert(buf[0] == '+' || buf[0] == '-'); // EPROTO
    dill_assert(buf[0] == '-'); // ECONNREFUSED
    s = dill_suffix_detach(s, deadline);
    dill_assert(s >= 0);
    return s;
}

