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

#ifndef DILL_XBUF_INCLUDED
#define DILL_XBUF_INCLUDED

struct dill_xbuf {
   int fd;
   void *txbuf;
   size_t txbufsz;
   size_t txhead;
   size_t txbytes;
   void *rxbuf;
   size_t rxbufsz;
   size_t rxhead;
   size_t rxbytes;
};

int dill_xbuf_init(
    struct dill_xbuf *self,
    int fd,
    size_t txbuf,
    size_t rxbuf);

void dill_xbuf_term(
    struct dill_xbuf *self);

int dill_xbuf_send(
    struct dill_xbuf *self,
    const void *buf,
    size_t len,
    int64_t deadline);

int dill_xbuf_recv(
    struct dill_xbuf *self,
    void *buf,
    size_t len,
    int64_t deadline);

int dill_xbuf_tofd(
    struct dill_xbuf *self);

#endif


