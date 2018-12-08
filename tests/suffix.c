/*

  Copyright (c) 2017 Martin Sustrik

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

#include <string.h>

#include "assert.h"
#include "../libdill.h"

coroutine void client(void) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", 5555, 0, -1);
    errno_assert(rc == 0);
    int s = tcp_connect(&addr, NULL, -1);
    errno_assert(s >= 0);
    rc = suffix_attachx(s, "\r\n", 2, NULL);
    errno_assert(rc == 0);
    rc = msend(s, "ABC", 3, -1);
    errno_assert(rc == 0);
    char buf[3];
    ssize_t sz = mrecv(s, buf, 3, -1);
    errno_assert(sz == 3);
    assert(buf[0] == 'G' && buf[1] == 'H' && buf[2] == 'I');
    rc = msend(s, "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = suffix_detachx(s);
    errno_assert(rc == 0);
    rc = hclose(s);
    errno_assert(rc == 0);
}

int main() {
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 5555, 0);
    errno_assert(rc == 0);
    int ls = tcp_listen(&addr, NULL);
    errno_assert(ls >= 0);
    int clh = go(client());
    int as = tcp_accept(ls, NULL, NULL, -1);
    errno_assert(as >= 0);

    rc = suffix_attachx(as, "\r\n", 2, NULL);
    errno_assert(rc == 0);
    char buf[16];
    ssize_t sz = mrecv(as, buf, sizeof(buf), -1);
    errno_assert(sz == 3);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = msend(as, "GHI", 3, -1);
    errno_assert(rc == 0);
    sz = mrecv(as, buf, sizeof(buf), -1);
    errno_assert(sz == 3);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = suffix_detachx(as);
    errno_assert(rc == 0);
    rc = hclose(as);
    errno_assert(rc == 0);

    int p[2];
    rc = ipc_pair(p, NULL, NULL);
    errno_assert(rc == 0);
    rc = suffix_attachx(p[0], "1234567890", 10, NULL);
    errno_assert(rc == 0);
    struct suffix_storage mem;
    struct suffix_opts opts = suffix_defaults;
    opts.mem = &mem;
    rc = suffix_attachx(p[1], "1234567890", 10, &opts);
    errno_assert(rc == 0);
    rc = msend(p[0], "First", 5, -1);
    errno_assert(rc == 0);
    rc = msend(p[0], "Second", 6, -1);
    errno_assert(rc == 0);
    rc = msend(p[0], "Third", 5, -1);
    errno_assert(rc == 0);
    sz = mrecv(p[1], buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 5 && memcmp(buf, "First", 5) == 0);
    struct iolist iol = {NULL, SIZE_MAX, NULL, 0};
    sz = mrecvl(p[1], &iol, &iol, -1);
    errno_assert(sz >= 0);
    sz = mrecv(p[1], buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 5 && memcmp(buf, "Third", 5) == 0);
    rc = msend(p[1], "Red", 3, -1);
    errno_assert(rc == 0);
    rc = msend(p[1], "Blue", 4, -1);
    errno_assert(rc == 0);
    rc = suffix_detachx(p[1]);
    errno_assert(rc == 0);
    sz = mrecv(p[0], buf, sizeof(buf), -1);
    errno_assert(sz == 3 && memcmp(buf, "Red", 3) == 0);
    sz = mrecv(p[0], buf, sizeof(buf), -1);
    errno_assert(sz == 4 && memcmp(buf, "Blue", 4) == 0);
    rc = suffix_detachx(p[0]);
    errno_assert(rc == 0);
    rc = hclose(p[1]);
    errno_assert(rc == 0);
    rc = hclose(p[0]);
    errno_assert(rc == 0);
    rc = bundle_wait(clh, -1);
    errno_assert(rc == 0);
    rc = hclose(clh);
    errno_assert(rc == 0);
    rc = hclose(ls);
    errno_assert(rc == 0);

    return 0;
}

