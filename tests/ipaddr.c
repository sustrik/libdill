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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>

#include "assert.h"
#include "../libdill.h"

int main(void) {

    /* Test conversion of string to IPv4 address and back again. */
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, "1.2.3.4", 5555, 0);
    errno_assert(rc == 0);
    int family = ipaddr_family(&addr);
    assert(family == AF_INET);
    const struct sockaddr_in *sin =
        (const struct sockaddr_in*)ipaddr_sockaddr(&addr);
    uint32_t a = *(uint32_t*)(&sin->sin_addr);
    assert(ntohl(a) == 0x01020304);
    assert(ntohs(sin->sin_port) == 5555);
    int port = ipaddr_port(&addr);
    assert(port == 5555);
    char buf[IPADDR_MAXSTRLEN];
    ipaddr_str(&addr, buf);
    assert(strcmp(buf, "1.2.3.4") == 0);

    /* Test conversion of string to IPv6 address and back again. */
    rc = ipaddr_local(&addr, "::1", 5555, 0);
    errno_assert(rc == 0);
    family = ipaddr_family(&addr);
    assert(family == AF_INET6);
    const struct sockaddr_in6 *sin6 =
        (const struct sockaddr_in6*)ipaddr_sockaddr(&addr);
    assert(ntohs(sin6->sin6_port) == 5555);
    port = ipaddr_port(&addr);
    assert(port == 5555);
    ipaddr_str(&addr, buf);
    assert(strcmp(buf, "::1") == 0);

    for(int hindex = 0; hindex < 4; hindex++) {
        static char *hostnames[] = {
            "large-dns-response-1k.lionet.info",
            "large-dns-response-2k.lionet.info",
            "large-dns-response-4k.lionet.info",
            "large-dns-response-8k.lionet.info"
        };

        rc = ipaddr_remote(&addr, hostnames[hindex], 80, 0, now() + 5000);
        if(rc != 0)
            assert(errno == ETIMEDOUT);
        ipaddr_str(&addr, buf);
        assert(strncmp("127.0.", buf, 6) == 0);
    }

    return 0;
}
