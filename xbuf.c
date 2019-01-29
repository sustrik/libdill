/*

  Copyright (c) 2019 Martin Sustrik

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

#include "fd.h"
#include "utils.h"
#include "xbuf.h"

#include <stdlib.h>

int dill_xbuf_init(struct dill_xbuf *self, int fd, size_t txbuf, size_t rxbuf) {
   self->fd = fd;
   self->txbuf = NULL;
   self->txbufsz = txbuf;
   self->txhead = 0;
   self->txbytes = 0;
   self->rxbuf = NULL;
   self->rxbufsz = rxbuf;
   self->rxhead = 0;
   self->rxbytes = 0;
   if(txbuf > 0) {
       self->txbuf = malloc(txbuf);
       if(dill_slow(!self->txbuf)) {
           errno = ENOMEM;
           return -1;
       }
   }
   if(txbuf > 0) {
       self->rxbuf = malloc(rxbuf);
       if(dill_slow(!self->rxbuf)) {
          free(self->txbuf);
          errno = ENOMEM;
          return -1;}
   }
   return 0;
}

void dill_xbuf_term(struct dill_xbuf *self) {
    if(self->txbuf) free(self->txbuf);
    if(self->rxbuf) free(self->rxbuf);
    dill_fd_close(self->fd);
}

int dill_xbuf_send(struct dill_xbuf *self,
      const void *buf, size_t len, int64_t deadline) {
    if(!self->txbuf) return dill_fd_send(self->fd, buf, len, deadline);
    dill_assert(0);
}

int dill_xbuf_recv(struct dill_xbuf *self,
      void *buf, size_t len, int64_t deadline) {
    if(!self->rxbuf) return dill_fd_recv(self->fd, buf, len, deadline);
    dill_assert(0);
}

int dill_xbuf_tofd(struct dill_xbuf *self) {
    if(self->txbuf) free(self->txbuf);
    if(self->rxbuf) free(self->rxbuf);
    if(self->txbuf || self->rbuf) {
        dill_fd_close(self->fd);
        errno = ENOTSUP;
        return -1;
    }
    return self->fd;
}

