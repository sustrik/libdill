/*

  Copyright (c) 2016 Martin Sustrik

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

#ifndef DILL_FD_INCLUDED
#define DILL_FD_INCLUDED

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdill.h"
#include "slist.h"

struct dill_ctx_fd {
    int count;
    struct dill_slist cache;
};

int dill_ctx_fd_init(struct dill_ctx_fd *ctx);
void dill_ctx_fd_term(struct dill_ctx_fd *ctx);

struct dill_fd_rxbuf {
    size_t len;
    size_t pos;
    uint8_t *buf;
};

void dill_fd_initrxbuf(
    struct dill_fd_rxbuf *rxbuf);
void dill_fd_termrxbuf(
    struct dill_fd_rxbuf *rxbuf);
int dill_fd_unblock(
    int s);
int dill_fd_connect(
    int s,
    const struct sockaddr *addr,
    socklen_t addrlen,
    int64_t deadline);
int dill_fd_accept(
    int s,
    struct sockaddr *addr,
    socklen_t *addrlen,
    int64_t deadline);
int dill_fd_send(
    int s,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline);
int dill_fd_recv(
    int s,
    struct dill_fd_rxbuf *rxbuf,
    struct dill_iolist *first,
    struct dill_iolist *last,
    int64_t deadline);
void dill_fd_close(
    int s);
int dill_fd_own(
    int s);
int dill_fd_check(
    int s,
    int type,
    int family1,
    int family2,
    int listening);

#endif

