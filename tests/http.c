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

#include "assert.h"
#include "../libdill.h"

int main(void) {
    int h[2];
    int rc = ipc_pair(h);
    assert(rc == 0);
    int s0 = http_attach(h[0]);
    assert(s0 >= 0);
    int s1 = http_attach(h[1]);
    /* Send request. */
    rc = http_sendrequest(s0, "GET", "/a/b/c", -1);
    errno_assert(rc == 0);
    /* Receive request. */
    char cmd[16];
    char url[16];
    rc = http_recvrequest(s1, cmd, sizeof(cmd), url, sizeof(url), -1);
    errno_assert(rc == 0);
    assert(strcmp(cmd, "GET") == 0);
    assert(strcmp(url, "/a/b/c") == 0);

    /* Test fields. */
    char name[32];
    char value[32];
    rc = http_sendfield(s0, "foo", "bar", -1);
    errno_assert(rc == 0);
    rc = http_recvfield(s1, name, sizeof(name), value, sizeof(value), -1);
    errno_assert(rc == 0);
    assert(strcmp(name, "foo") == 0);
    assert(strcmp(value, "bar") == 0);

    rc = http_sendfield(s0, "baz", "quux", -1);
    errno_assert(rc == 0);
    rc = http_recvfield(s1, name, sizeof(name), value, sizeof(value), -1);
    errno_assert(rc == 0);
    assert(strcmp(name, "baz") == 0);
    assert(strcmp(value, "quux") == 0);

    rc = http_sendfield(s0, "Date", "Tue, 15 Nov 1994 08:12:31 GMT", -1);
    errno_assert(rc == 0);
    rc = http_recvfield(s1, name, sizeof(name), value, sizeof(value), -1);
    errno_assert(rc == 0);
    assert(strcmp(name, "Date") == 0);
    assert(strcmp(value, "Tue, 15 Nov 1994 08:12:31 GMT") == 0);

    rc = http_sendfield(s0, "invalid field name ", "bar", -1);
    assert(rc == -1 && errno == EINVAL);

    rc = http_sendfield(s0, "valid-field-name", "  rpad", -1);
    errno_assert(rc == 0);
    rc = http_recvfield(s1, name, sizeof(name), value, sizeof(value), -1);
    errno_assert(rc == 0);
    assert(strcmp(name, "valid-field-name") == 0);
    assert(strcmp(value, "rpad") == 0);

    rc = http_sendfield(s0, "valid-field-name", "lpad  ", -1);
    errno_assert(rc == 0);
    rc = http_recvfield(s1, name, sizeof(name), value, sizeof(value), -1);
    errno_assert(rc == 0);
    assert(strcmp(name, "valid-field-name") == 0);
    assert(strcmp(value, "lpad") == 0);

    rc = http_sendfield(s0, "valid-field-name", "  both pad  ", -1);
    errno_assert(rc == 0);
    rc = http_recvfield(s1, name, sizeof(name), value, sizeof(value), -1);
    errno_assert(rc == 0);
    assert(strcmp(name, "valid-field-name") == 0);
    assert(strcmp(value, "both pad") == 0);

    rc = http_done(s0, -1);
    errno_assert(rc == 0);
    rc = http_recvfield(s1, name, sizeof(name), value, sizeof(value), -1);
    assert(rc < 0 && errno == EPIPE);

    /* Send response. */
    rc = http_sendstatus(s1, 200, "OK", -1);
    errno_assert(rc == 0);
    rc = http_sendfield(s1, "one", "two", -1);
    errno_assert(rc == 0);
    rc = http_done(s1, -1);
    errno_assert(rc == 0);
    /* Receive response. */
    char reason[16];
    rc = http_recvstatus(s0, reason, sizeof(reason), -1);
    assert(rc == 200);
    assert(strcmp(reason, "OK") == 0);
    rc = http_recvfield(s0, name, sizeof(name), value, sizeof(value), -1);
    errno_assert(rc == 0);
    assert(strcmp(name, "one") == 0);
    assert(strcmp(value, "two") == 0);
    rc = http_recvfield(s0, name, sizeof(name), value, sizeof(value), -1);
    assert(rc < 0 && errno == EPIPE);
    /* Close the sockets. */
    h[0] = http_detach(s1, -1);
    assert(h[0] >= 0);
    h[1] = http_detach(s0, -1);
    errno_assert(h[1] >= 0);
    rc = hclose(h[1]);
    errno_assert(rc == 0);
    rc = hclose(h[0]);
    errno_assert(rc == 0);
    return 0;
}

