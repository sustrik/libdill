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

#ifndef DILL_TESTS_PROTOCOL_H_INCLUDED
#define DILL_TESTS_PROTOCOL_H_INCLUDED

#include "../libdillimpl.h"

#include "assert.h"

void protocol_check_bsock(int s) {
    void *p = hquery(s, bsock_type);
    errno_assert(p);
    p = hquery(s, msock_type);
    assert(!p && errno == ENOTSUP);
}

void protocol_check_msock(int s) {
    void *p = hquery(s, msock_type);
    errno_assert(p);
    p = hquery(s, bsock_type);
    assert(!p && errno == ENOTSUP);
}

void protocol_check_nosock(int s) {
    void *p = hquery(s, bsock_type);
    assert(!p && errno == ENOTSUP);
    p = hquery(s, msock_type);
    assert(!p && errno == ENOTSUP);
}
#endif

