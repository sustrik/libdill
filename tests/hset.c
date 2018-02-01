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

#include "assert.h"
#include "../libdill.h"

coroutine void worker1(void) {
}

coroutine void worker2(void) {
    int rc = msleep(now() + 100000);
    assert(rc == -1 && errno == ECANCELED);
}

coroutine void worker3(void) {
    int rc = msleep(now() + 50);
    assert(rc == 0);
}

int main(void) {
    int hs1 = hset();
    errno_assert(hs1 >= 0);
    int rc = hclose(hs1);
    errno_assert(rc == 0);

    int hs2 = hset();
    errno_assert(hs2 >= 0);
    rc = go_set(hs2, worker1());
    errno_assert(rc == 0);
    rc = hclose(hs2);
    errno_assert(rc == 0);

    int hs3 = hset();
    errno_assert(hs3 >= 0);
    rc = go_set(hs3, worker1());
    errno_assert(rc == 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hs3);
    errno_assert(rc == 0);

    int hs4 = hset();
    errno_assert(hs4 >= 0);
    rc = go_set(hs4, worker2());
    errno_assert(rc == 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hs4);
    errno_assert(rc == 0);

    int hs5 = hset();
    errno_assert(hs5 >= 0);
    rc = go_set(hs5, worker3());
    errno_assert(rc == 0);
    rc = go_set(hs5, worker3());
    errno_assert(rc == 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hs5);
    errno_assert(rc == 0);

    int hs6 = hset();
    errno_assert(hs6 >= 0);
    rc = go_set(hs6, worker2());
    errno_assert(rc == 0);
    rc = go_set(hs6, worker2());
    errno_assert(rc == 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hs6);
    errno_assert(rc == 0);

    return 0;
}
