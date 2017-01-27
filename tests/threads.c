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

#include <stdio.h>
#include <pthread.h>

#include "assert.h"
#include "../libdill.h"

coroutine void worker(int count, const char *text) {
    int i;
    for(i = 0; i != count; ++i) {
        printf("%s\n", text);
        int rc = msleep(now() + 10);
        if(rc < 0 && errno == ECANCELED) return;
        errno_assert(rc == 0);
    }
}

/* This function is run by thread t1. */
void *threadmain(void *arg)
{
    int cr1 = go(worker(4, "a "));
    errno_assert(cr1 >= 0);
    int cr2 = go(worker(2, "b"));
    errno_assert(cr2 >= 0);
    int cr3 = go(worker(3, "c"));
    errno_assert(cr3 >= 0);
    int rc = msleep(now() + 200);
    errno_assert(rc == 0);
    rc = hclose(cr1);
    errno_assert(rc == 0);
    rc = hclose(cr2);
    errno_assert(rc == 0);
    rc = hclose(cr3);
    errno_assert(rc == 0);
    return NULL;
}

int main() {
    pthread_t t1;
    int rc = pthread_create(&t1, NULL, threadmain, NULL);
    errno_assert(rc == 0);
    int cr1 = go(worker(4, "a "));
    errno_assert(cr1 >= 0);
    int cr2 = go(worker(2, "b"));
    errno_assert(cr2 >= 0);
    int cr3 = go(worker(3, "c"));
    errno_assert(cr3 >= 0);
    rc = msleep(now() + 200);
    errno_assert(rc == 0);
    rc = hclose(cr1);
    errno_assert(rc == 0);
    rc = hclose(cr2);
    errno_assert(rc == 0);
    rc = hclose(cr3);
    errno_assert(rc == 0);
    rc = pthread_join(t1, NULL);
    errno_assert(rc == 0);
    return 0;
}
