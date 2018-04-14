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
#include <stdlib.h>
#include <unistd.h>

#include "../../libdill.h"

coroutine void dialogue(int s) {
    int rc = msend(s, "What's your name?", 17, -1);
    if(rc != 0) goto cleanup;
    char inbuf[256];
    ssize_t sz = mrecv(s, inbuf, sizeof(inbuf), -1);
    if(sz < 0) goto cleanup;
    inbuf[sz] = 0;
    char outbuf[256];
    rc = snprintf(outbuf, sizeof(outbuf), "Hello, %s!", inbuf);
    rc = msend(s, outbuf, rc, -1);
    if(rc != 0) goto cleanup;
cleanup:
    rc = hclose(s);
    assert(rc == 0);
}

int main(int argc, char *argv[]) {

    int port = 5555;
    if(argc > 1)
        port = atoi(argv[1]);

    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, port, 0);
    assert(rc == 0);
    int ls = tcp_listen(&addr, 10);
    if(ls < 0) {
        perror("Can't open listening socket");
        return 1;
    }

    while(1) {
        int s = tcp_accept(ls, NULL, -1);
        assert(s >= 0);
        s = suffix_attach(s, "\r\n", 2);
        assert(s >= 0);
        int cr = go(dialogue(s));
        assert(cr >= 0);
    }
}

