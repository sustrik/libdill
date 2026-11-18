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

#include <stdint.h>
#include <string.h>

#include "assert.h"
#include "../libdill.h"

coroutine void nohttp_client(int s) {
    s = ws_attach_client(s, WS_NOHTTP | WS_TEXT, NULL, NULL, -1);
    errno_assert(s >= 0);
    int rc = msend(s, "ABC", 3, -1);
    errno_assert(rc == 0);
    char buf[3];
    ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 3);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    sz = mrecv(s, buf, sizeof(buf), -1);
    assert(sz < 0 && errno == EPIPE);
    int status;
    sz = ws_status(s, &status, buf, sizeof(buf));
    errno_assert(sz >= 0);
    assert(status == 1000);
    assert(sz == 2);
    assert(buf[0] == 'O' && buf[1] == 'K');
    s = ws_detach(s, 0, NULL, 0, -1);
    errno_assert(s >= 0);
    rc = hclose(s);
    errno_assert(rc == 0);
}

coroutine void http_client(int s) {
    s = ws_attach_client(s, WS_BINARY, "/", "www.example.org", -1);
    errno_assert(s >= 0);
    int rc = msend(s, "ABC", 3, -1);
    errno_assert(rc == 0);
    char buf[3];
    ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 3);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    sz = mrecv(s, buf, sizeof(buf), -1);
    assert(sz < 0 && errno == EPIPE);
    int status;
    sz = ws_status(s, &status, buf, sizeof(buf));
    errno_assert(sz >= 0);
    assert(status == 1000);
    assert(sz == 0);
    s = ws_detach(s, 0, NULL, 0, -1);
    errno_assert(s >= 0);
    rc = hclose(s);
    errno_assert(rc == 0);
}

/* Returns a WebSocket attached to one end of a socket pair, with 'input'
   already written to the other end, which is returned in 'peer'. */
static int ws_with_input(int server, const uint8_t *input, size_t len,
      int *peer) {
    int p[2];
    int rc = ipc_pair(p);
    errno_assert(rc == 0);
    int s = server ?
        ws_attach_server(p[0], WS_NOHTTP | WS_BINARY, NULL, 0, NULL, 0, -1) :
        ws_attach_client(p[0], WS_NOHTTP | WS_BINARY, NULL, NULL, -1);
    errno_assert(s >= 0);
    rc = bsend(p[1], input, len, -1);
    errno_assert(rc == 0);
    *peer = p[1];
    return s;
}

static void close_ws_and_peer(int s, int peer) {
    int rc = hclose(s);
    errno_assert(rc == 0);
    rc = hclose(peer);
    errno_assert(rc == 0);
}

/* After a protocol violation the rest of the stream is not parsed. Here
   the payload of the offending frame would pass for a frame of its own. */
static void test_recv_fails_after_protocol_error(void) {
    /* An unmasked frame, which a server must reject. Its payload is the
       masked frame of a 1-byte message. */
    const uint8_t input[] = {0x82, 0x07,
        0x82, 0x81, 0x01, 0x02, 0x03, 0x04, 'X' ^ 0x01};
    int peer;
    int s = ws_with_input(1, input, sizeof(input), &peer);
    char buf[16];
    ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
    assert(sz == -1 && errno == EPROTO);
    sz = mrecv(s, buf, sizeof(buf), -1);
    assert(sz == -1 && errno == ECONNRESET);
    close_ws_and_peer(s, peer);
}

/* Same thing after a message that does not fit into the buffer. */
static void test_recv_fails_after_oversized_msg(void) {
    /* A message of 4 bytes. The last 3 are the frame of a 1-byte message. */
    const uint8_t input[] = {0x82, 0x04, 'A', 0x82, 0x01, 'X'};
    int peer;
    int s = ws_with_input(0, input, sizeof(input), &peer);
    char buf[1];
    ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
    assert(sz == -1 && errno == EMSGSIZE);
    sz = mrecv(s, buf, sizeof(buf), -1);
    assert(sz == -1 && errno == ECONNRESET);
    close_ws_and_peer(s, peer);
}

/* A close frame that is invalid, or not received in full, does not count
   as the peer closing the connection. */
static void test_bad_close_frame_is_not_a_close(void) {
    const uint8_t bad_status[] = {0x88, 0x02, 999 >> 8, 999 & 0xff};
    int peer;
    int s = ws_with_input(0, bad_status, sizeof(bad_status), &peer);
    char buf[16];
    ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
    assert(sz == -1 && errno == EPROTO);
    sz = ws_status(s, NULL, NULL, 0);
    assert(sz == -1 && errno == EAGAIN);
    sz = mrecv(s, buf, sizeof(buf), -1);
    assert(sz == -1 && errno == ECONNRESET);
    close_ws_and_peer(s, peer);

    const uint8_t truncated[] = {0x88, 0x02, 1000 >> 8};
    s = ws_with_input(0, truncated, sizeof(truncated), &peer);
    sz = mrecv(s, buf, sizeof(buf), now() + 50);
    assert(sz == -1 && errno == ETIMEDOUT);
    sz = ws_status(s, NULL, NULL, 0);
    assert(sz == -1 && errno == EAGAIN);
    close_ws_and_peer(s, peer);
}

int main(void) {
    int p[2];
    int rc = ipc_pair(p);
    errno_assert(rc == 0);
    int cr = go(nohttp_client(p[1]));
    errno_assert(cr >= 0);
    int s = ws_attach_server(p[0], WS_NOHTTP | WS_TEXT, NULL, 0, NULL, 0, -1);
    errno_assert(s >= 0);
    char buf[3];
    ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 3);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = msend(s, "DEF", 3, -1);
    errno_assert(rc == 0);
    s = ws_detach(s, 1000, "OK", 2, -1);
    errno_assert(s >= 0);
    rc = hclose(s);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    rc = ipc_pair(p);
    errno_assert(rc == 0);
    cr = go(http_client(p[1]));
    errno_assert(cr >= 0);
    char resource[256];
    char host[256];
    s = ws_attach_server(p[0], WS_BINARY, resource, sizeof(resource),
        host, sizeof(host), -1);
    errno_assert(s >= 0);
    assert(strcmp(resource, "/") == 0);
    assert(strcmp(host, "www.example.org") == 0);
    sz = mrecv(s, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 3);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = msend(s, "DEF", 3, -1);
    errno_assert(rc == 0);
    s = ws_detach(s, 1000, NULL, 0, -1);
    errno_assert(s >= 0);
    rc = hclose(s);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    test_recv_fails_after_protocol_error();
    test_recv_fails_after_oversized_msg();
    test_bad_close_frame_is_not_a_close();
    
    return 0;
}

