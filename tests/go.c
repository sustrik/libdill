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

#include <assert.h>
#include <errno.h>
#include <stdio.h>

#include "../libdill.h"

coroutine int dummy(void) {
    int rc = msleep(now() + 50);
    assert(rc == 0);
    return 0;
}

int sum = 0;

coroutine int worker(int count, int n) {
    int i;
    for(i = 0; i != count; ++i) {
        sum += n;
        int rc = yield();
        assert(rc == 0);
    }
    return 0;
}

static int worker2_done = 0;

coroutine int worker2(void) {
    int rc = msleep(now() + 1000);
    assert(rc == -1 && errno == ECANCELED);
    /* Try again to test whether subsequent calls fail as well. */
    rc = msleep(now() + 1000);
    assert(rc == -1 && errno == ECANCELED);
    ++worker2_done;
    return 0;
}

static int worker3_done = 0;

coroutine int worker3(void) {
    int rc = msleep(now() + 100);
    assert(rc == 0);
    ++worker3_done;
    return 0;
}

static int worker4_finished = 0;
static int worker4_canceled = 0;

coroutine int worker4(int64_t deadline) {
    int rc = msleep(deadline);
    if(rc == 0) {
        ++worker4_finished;
        return 0;
    }
    if(rc == -1 && errno == ECANCELED) {
        ++worker4_canceled;
        return 0;
    }
    assert(0);
}

coroutine int worker6(void) {
    int rc = msleep(now() + 2000);
    assert(rc == -1 && errno == ECANCELED);
    return 0;
}

coroutine int worker5(void) {
    int cr = go(worker6());
    assert(cr >= 0);
    stop(cr);
    return 0;
}

int main() {
    int cr1 = go(worker(3, 7));
    assert(cr1 >= 0);
    int cr2 = go(worker(1, 11));
    assert(cr2 >= 0);
    int cr3 = go(worker(2, 5));
    assert(cr3 >= 0);
    int rc = stop(hndls3, 3, -1);
    assert(rc == 0);
    assert(sum == 42);

    /* Test whether stack deallocation works. */
    int i;
    int hndls2[20];
    for(i = 0; i != 20; ++i) {
        hndls2[i] = go(dummy());
        assert(hndls2[i] >= 0);
    }
    rc = msleep(now() + 100);
    assert(rc == 0);
    rc = stop(hndls2, 20, -1);
    assert(rc == 0);

    int hndls[6];

    /* Test whether immediate cancelation works. */
    hndls[0] = go(worker2());
    assert(hndls[0] >= 0);
    hndls[1] = go(worker2());
    assert(hndls[1] >= 0);
    hndls[2] = go(worker2());
    assert(hndls[2] >= 0);
    rc = msleep(now() + 30);
    assert(rc == 0);
    rc = stop(hndls, 3, 0);
    assert(rc == 0);
    assert(worker2_done == 3);

    /* Test cancelation with infinite deadline. */
    hndls[0] = go(worker3());
    assert(hndls[0] >= 0);
    hndls[1] = go(worker3());
    assert(hndls[1] >= 0);
    hndls[2] = go(worker3());
    assert(hndls[2] >= 0);
    rc = stop(hndls, 3, -1);
    assert(rc == 0);
    assert(worker3_done == 3);

    /* Test cancelation with a finite deadline. */
    hndls[0] = go(worker4(now() + 50));
    assert(hndls[0] >= 0);
    hndls[1] = go(worker4(now() + 50));
    assert(hndls[1] >= 0);
    hndls[2] = go(worker4(now() + 50));
    assert(hndls[2] >= 0);
    hndls[3] = go(worker4(now() + 200));
    assert(hndls[3] >= 0);
    hndls[4] = go(worker4(now() + 200));
    assert(hndls[4] >= 0);
    hndls[5] = go(worker4(now() + 200));
    assert(hndls[5] >= 0);
    rc = stop(hndls, 6, now() + 100);
    assert(rc == 0);
    assert(worker4_finished == 3);
    assert(worker4_canceled == 3);

    /* Test canceling a cancelation. */
    int hndl = go(worker5());
    assert(hndl >= 0);
    rc = stop(&hndl, 1, now() + 50);
    assert(rc == 0);

    /* Let the test running for a while to detect possible errors in
       independently running coroutines. */
    rc = msleep(now() + 100);
    assert(rc == 0);

    return 0;
}

