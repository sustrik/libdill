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

coroutine void worker3(int delay) {
    int rc = msleep(now() + delay);
    assert(rc == 0);
}

int main(void) {
    int hndl1 = bundle();
    errno_assert(hndl1 >= 0);
    int rc = hclose(hndl1);
    errno_assert(rc == 0);

    int hndl2 = bundle();
    errno_assert(hndl2 >= 0);
    rc = bundle_go(hndl2, worker1());
    errno_assert(rc == 0);
    rc = hclose(hndl2);
    errno_assert(rc == 0);

    int hndl3 = bundle();
    errno_assert(hndl3 >= 0);
    rc = bundle_go(hndl3, worker1());
    errno_assert(rc == 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hndl3);
    errno_assert(rc == 0);

    int hndl4 = bundle();
    errno_assert(hndl4 >= 0);
    rc = bundle_go(hndl4, worker2());
    errno_assert(rc == 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hndl4);
    errno_assert(rc == 0);

    int hndl5 = bundle();
    errno_assert(hndl5 >= 0);
    rc = bundle_go(hndl5, worker3(50));
    errno_assert(rc == 0);
    rc = bundle_go(hndl5, worker3(50));
    errno_assert(rc == 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hndl5);
    errno_assert(rc == 0);

    int hndl6 = bundle();
    errno_assert(hndl6 >= 0);
    rc = bundle_go(hndl6, worker2());
    errno_assert(rc == 0);
    rc = bundle_go(hndl6, worker2());
    errno_assert(rc == 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hndl6);
    errno_assert(rc == 0);

    int hndl7 = bundle();
    errno_assert(hndl7 >= 0);
    char stk[4096];
    rc = bundle_go_mem(hndl7, worker1(), stk, sizeof(stk));
    errno_assert(rc == 0);
    rc = hclose(hndl7);
    errno_assert(rc == 0);

    /* Test creating bundle via go() function. */
    int hndl8 = go(worker1());
    errno_assert(hndl8 >= 0);
    rc = bundle_go(hndl8, worker1());
    errno_assert(rc == 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hndl8);
    errno_assert(rc == 0);

    /* Test waiting on empty bundle. */
    int hndl9 = bundle();
    errno_assert(hndl9 >= 0);
    rc = bundle_wait(hndl9, -1);
    errno_assert(rc == 0);
    rc = hclose(hndl9);
    errno_assert(rc == 0);

    /* Test waiting on bundle with one coroutine. */
    int hndl10 = go(worker3(50));
    errno_assert(hndl10 >= 0);
    rc = bundle_wait(hndl10, -1);
    errno_assert(rc == 0);
    rc = hclose(hndl10);
    errno_assert(rc == 0);

    /* Test waiting on bundle with two coroutines. */
    int hndl11 = bundle();
    errno_assert(hndl11 >= 0);
    rc = bundle_go(hndl11, worker3(50));
    errno_assert(rc == 0);
    rc = bundle_go(hndl11, worker3(100));
    errno_assert(rc == 0);
    rc = bundle_wait(hndl11, -1);
    errno_assert(rc == 0);
    rc = bundle_go(hndl11, worker1());
    errno_assert(rc == 0);
    rc = hclose(hndl11);
    errno_assert(rc == 0);

    /* Test waiting on bundle with timout. */
    int hndl12 = go(worker3(50));
    errno_assert(hndl12 >= 0);
    rc = bundle_wait(hndl12, now() + 1000);
    errno_assert(rc == 0);
    rc = hclose(hndl12);
    errno_assert(rc == 0);
    int hndl13 = go(worker2());
    errno_assert(hndl13 >= 0);
    rc = bundle_wait(hndl13, now() + 50);
    errno_assert(rc == -1 && errno == ETIMEDOUT);
    rc = hclose(hndl13);
    errno_assert(rc == 0);

    return 0;
}
