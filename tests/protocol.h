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

#include <string.h>

#include "../libdillimpl.h"

#include "utils.h"

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

coroutine void protocol_bclient(int s) {
    /* Simple handshake. */
    char buf[3];
    int rc = bsend(s, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = brecv(s, buf, 3, -1);
    errno_assert(rc == 0);
    assert(memcmp(buf, "DEF", 3) == 0);
    /* Receive large amount of data. */
    uint8_t c = 0;
    int i;
    for(i = 0; i != 2777; ++i) {
        uint8_t b[257];
        rc = brecv(s, b, sizeof(b), -1);
        errno_assert(rc == 0);
        int j;
        for(j = 0; j != sizeof(b); ++j) {
            assert(b[j] == c);
            c++;
        }
    }
}

void protocol_btest(int s1, int s2) {
    int clnt = go(protocol_bclient(s2));
    errno_assert(clnt >= 0);
    /* Simple handshake. */
    char buf[3];
    int rc = brecv(s1, buf, 3, -1);
    errno_assert(rc == 0);
    assert(memcmp(buf, "ABC", 3) == 0);
    rc = bsend(s1, "DEF", 3, -1);
    /* Send large amount of data. */
    uint8_t c = 0;
    uint8_t b[2777];
    int i;
    for(i = 0; i != 257; ++i) {
        int j;
        for(j = 0; j != sizeof(b); j++) {
            b[j] = c;
            c++;
        }
        int rc = bsend(s1, b, sizeof(b), -1);
        errno_assert(rc == 0);
    }
    rc = bundle_wait(clnt, -1);
    errno_assert(rc == 0);
    rc = hclose(clnt);
    errno_assert(rc == 0);
}

coroutine void protocol_mclient(int s) {
}

void protocol_mtest(int s1, int s2) {
}

#endif

