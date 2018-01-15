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

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "assert.h"
#include "../libdill.h"

coroutine static void delay(int n, int ch) {
    int rc = msleep(now() + n);
    errno_assert(rc == 0);
    rc = chsend(ch, &n, sizeof(n), -1);
    errno_assert(rc == 0);
}

static int canceled = 0;

coroutine static void canceled_delay(int n) {
    int rc = msleep(now() + n);
    assert(rc < 0);
    errno_assert(errno == ECANCELED);
    canceled = 1;
}

int main() {
    /* Test 'msleep'. */
    int64_t deadline = now() + 100;
    int rc = msleep(deadline);
    errno_assert(rc == 0);
    int64_t diff = now () - deadline;
    time_assert(diff, 0);

    /* msleep-sort */
    int ch[2];
    rc = chmake(ch);
    errno_assert(rc == 0);
    int hndls[4];
    hndls[0] = go(delay(30, ch[0]));
    errno_assert(hndls[0] >= 0);
    hndls[1] = go(delay(40, ch[0]));
    errno_assert(hndls[1] >= 0);
    hndls[2] = go(delay(10, ch[0]));
    errno_assert(hndls[2] >= 0);
    hndls[3] = go(delay(20, ch[0]));
    errno_assert(hndls[3] >= 0);
    int val;
    rc = chrecv(ch[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 10);
    rc = chrecv(ch[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 20);
    rc = chrecv(ch[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 30);
    rc = chrecv(ch[1], &val, sizeof(val), -1);
    errno_assert(rc == 0);
    assert(val == 40);
    rc = hclose(hndls[0]);
    errno_assert(rc == 0);
    rc = hclose(hndls[1]);
    errno_assert(rc == 0);
    rc = hclose(hndls[2]);
    errno_assert(rc == 0);
    rc = hclose(hndls[3]);
    errno_assert(rc == 0);
    rc = hclose(ch[0]);
    errno_assert(rc == 0);
    rc = hclose(ch[1]);
    errno_assert(rc == 0);

    /* Test cancelling msleep. */
    int hndl = go(canceled_delay(1000));
    errno_assert(hndl >= 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(hndl);
    errno_assert(rc == 0);
    assert(canceled == 1);

    return 0;
}

