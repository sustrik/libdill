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

#include "assert.h"
#include "../libdillimpl.h"

static int status = 0;

struct test {
    struct hvfs vfs;
};

static void *test_query(struct hvfs *vfs, const void *type) {
    status = 1;
    return &status;
}

static void test_close(struct hvfs *vfs) {
    status = 2;
}

int main(void) {

    struct test t;
    t.vfs.query = test_query;
    t.vfs.close = test_close;
    int h = hmake(&t.vfs);
    errno_assert(h >= 0);
    void *p = hquery(h, NULL);
    errno_assert(p);
    assert(p == &status);
    assert(status == 1);
    int rc = hclose(h);
    errno_assert(rc == 0);
    assert(status == 2);

    int ch[2];
    rc = chmake(ch);
    errno_assert(rc == 0);
    ch[0] = hown(ch[0]);
    errno_assert(ch[0] >= 0);
    ch[1] = hown(ch[1]);
    errno_assert(ch[1] >= 0);
    rc = hclose(ch[0]);
    errno_assert(rc == 0);
    rc = hclose(ch[1]);
    errno_assert(rc == 0);

    return 0;
}

