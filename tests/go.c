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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "assert.h"
#include "../libdill.h"

coroutine void dummy(void) {
    int rc = msleep(now() + 50);
    errno_assert(rc == 0);
}

int sum = 0;

coroutine void worker(int count, int n) {
    int i;
    for(i = 0; i != count; ++i) {
        sum += n;
        int rc = yield();
        errno_assert(rc == 0);
    }
}

static int worker2_done = 0;

coroutine void worker2(void) {
    int rc = msleep(now() + 1000);
    assert(rc == -1 && errno == ECANCELED);
    /* Try again to test whether subsequent calls fail as well. */
    rc = msleep(now() + 1000);
    assert(rc == -1 && errno == ECANCELED);
    ++worker2_done;
}

coroutine void worker6(void) {
    int rc = msleep(now() + 2000);
    assert(rc == -1 && errno == ECANCELED);
}

int main() {
    /* Basic test. Run some coroutines. */
    int cr1 = go(worker(3, 7));
    errno_assert(cr1 >= 0);
    int cr2 = go(worker(1, 11));
    errno_assert(cr2 >= 0);
    int cr3 = go(worker(2, 5));
    errno_assert(cr3 >= 0);
    int rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(cr1);
    errno_assert(rc == 0);
    rc = hclose(cr2);
    errno_assert(rc == 0);
    rc = hclose(cr3);
    errno_assert(rc == 0);
    assert(sum == 42);

    /* Test whether stack deallocation works. */
    int i;
    int hndls2[20];
    for(i = 0; i != 20; ++i) {
        hndls2[i] = go(dummy());
        errno_assert(hndls2[i] >= 0);
    }
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    for(i = 0; i != 20; ++i) {
        rc = hclose(hndls2[i]);
        errno_assert(rc == 0);
    }

    /* Test whether immediate cancelation works. */
    cr1 = go(worker2());
    errno_assert(cr1 >= 0);
    cr2 = go(worker2());
    errno_assert(cr2 >= 0);
    cr3 = go(worker2());
    errno_assert(cr3 >= 0);
    rc = msleep(now() + 30);
    errno_assert(rc == 0);
    rc = hclose(cr1);
    errno_assert(rc == 0);
    rc = hclose(cr2);
    errno_assert(rc == 0);
    rc = hclose(cr3);
    errno_assert(rc == 0);
    assert(worker2_done == 3);

    /* Let the test running for a while to detect possible errors if there
       was a bug that left any corotines running. */
    rc = msleep(now() + 100);
    errno_assert(rc == 0);

    /* Test go_stack. */
    char *stack = malloc(4096);
    assert(stack);
    cr1 = go_stack(dummy(), stack, 4096);
    errno_assert(cr1 >= 0);
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = hclose(cr1);
    errno_assert(rc == 0);
    free(stack);

    return 0;
}

