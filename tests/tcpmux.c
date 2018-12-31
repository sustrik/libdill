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

#include "utils.h"
#include "../libdill.h"

coroutine void client(void) {
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, "127.0.0.1", 10001, 0);
    errno_assert(rc == 0);
    int i;
    for(i = 0; i != 10; i++) {
        int s = tcp_connect(&addr, NULL, -1);
        if(s < 0 && errno == ECONNREFUSED) {
            msleep(now() + 5000);
            continue;
        }
        errno_assert(s >= 0);
        rc = tcpmux_switch(s, "foo", -1);
        if(rc != 0 && errno == ECONNREFUSED) {
            msleep(now() + 5000);
            continue;
        }
        errno_assert(rc == 0);
        printf("connected\n");

        rc = hclose(s);
        errno_assert(rc == 0);
    }
}

coroutine void run_daemon(void) {
    int rc = tcpmux_daemon(NULL);
    errno_assert(rc == -1 && errno == ECANCELED);
}

int main(void) {
    int dmn = go(run_daemon());
    errno_assert(dmn >= 0);

    int rc = msleep(now() + 100);
    errno_assert(rc == 0);

    int lst = tcpmux_listen("foo", NULL, -1);
    errno_assert(lst >= 0);
    go(client());
    int i;
    for(i = 0; i != 10; i++) {
        int s = tcpmux_accept(lst, NULL, NULL, -1);
        errno_assert(s >= 0);
        printf("accepted\n");
        hclose(s);
    }

    rc = hclose(dmn);
    errno_assert(rc == 0);

    return 0;
}
