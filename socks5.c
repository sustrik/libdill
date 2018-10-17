/*

  Copyright (c) 2018 Joseph deBlaquiere

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
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "utils.h"

static int dill_socks5_clientauth(int s, const char *username, const char *password,
      int64_t deadline) {
    // ensure socket is bytesstream
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) return -1;
    // validate input, specifically user and pass, which may be NULL
    if(username) {
        if(strlen(username) > 255) {errno = EINVAL; return -1;}
    }
    if(password) {
        if(strlen(password) > 255) {errno = EINVAL; return -1;}
    }
    // version identifier/method selection request
    // client passes list of desired auth methods
    uint8_t vims[4];
    size_t vims_len;
    vims[0] = 0x05; // VER = 5
    vims[2] = 0x00; // METHOD = NO AUTH REQ'D
    vims[3] = 0x02; // METHOD = USERNAME/PASSWORD
    // TODO: Add support for GSSAPI Auth
    // if we don't have both username and password only request NO AUTH
    if((!username) || (!password)) {
        vims[1] = 0x01; // NMETHODS = 1 (NO AUTH)
        vims_len = 3;
    } else {
        vims[1] = 0x02; // NMETHODS = 2 (NO AUTH or USER/PASS)
        vims_len = 4;
    }
    //for (int i = 0; i < vims_len; i++) {printf("%02X ", vims[i]);} printf(">\n");
    int err = dill_bsend(s, (void *)vims, vims_len, deadline);
    if(dill_slow(err)) return -1;
    // version identifier/method response
    // server responds with chosen auth method (or error)
    uint8_t vimr[2];
    err = dill_brecv(s, (void *)vimr, 2, deadline);
    if(dill_slow(err)) return -1;
    //for (int i = 0; i < 2; i++) {printf("%02X ", vimr[i]);} printf("<\n");
    // validate VER in response
    if(dill_slow(vimr[0] != 0x05)) {errno = EPROTO; return -1;}
    switch(vimr[1]) {
        case 0x00:
            // proxy accepted NO AUTH REQ'D, so we're done
            return 0;
        case 0x02:
            dill_assert(!!username);
            dill_assert(!!password);
            // USER/PASS AUTH - per RFC 1929
            // max USER, PASS is 255 bytes (plus 3 bytes for VER, ULEN, PLEN)
            uint8_t upauth[513];
            // previously range checked, safe to cast to uint8_t
            uint8_t ulen = (uint8_t)strlen(username);
            uint8_t plen = (uint8_t)strlen(password);
            upauth[0] = 0x01; // VER = 1
            upauth[1] = ulen; // ULEN
            strncpy((char *)(upauth + 2), username, ulen); // UNAME
            upauth[2 + ulen] = plen; // PLEN
            strncpy((char *)(upauth + 3 + ulen), password, plen); // PASSWD
            //for (int i = 0; i < 3+ulen+plen; i++) {printf("%02X ", upauth[i]);} printf(">\n");
            err = dill_bsend(s, (void *)upauth, 3 + ulen + plen, deadline);
            if(dill_slow(err)) return -1;
            uint8_t upauthr[2];
            err = dill_brecv(s, (void *)upauthr, 2, deadline);
            if(dill_slow(err)) return -1;
            //for (int i = 0; i < 2; i++) {printf("%02X ", upauthr[i]);} printf("<\n");
            if(dill_slow(upauthr[0] != 0x01)) {errno = EPROTO; return -1;}
            if(dill_slow(upauthr[1] != 0x00)) {errno = EACCES; return -1;}
            return 0;
        case 0xFF:
            errno = EACCES; return -1;
        default:
            // something is fishy as the proxy accepted something not requested
            dill_assert(0);
    }
    return -1;
}

// convenience functions for handling socks5 address types

#define S5ADDR_IPV4 (1)
#define S5ADDR_IPV6 (4)
#define S5ADDR_NAME (3)

typedef struct {
    uint8_t atyp;
    uint8_t addr[256];
    size_t  alen;
    int     port;
} _socks5_addr;

static void _s5_ipaddr_to_s5_addr(_socks5_addr *s5, struct dill_ipaddr *addr) {
    if(dill_ipaddr_family(addr) == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)addr;
        s5->atyp = S5ADDR_IPV4;
        s5->alen = sizeof(ipv4->sin_addr);
        s5->port = dill_ipaddr_port(addr);
        dill_assert(s5->alen == 4);
        memcpy((void *)s5->addr, (void *)&ipv4->sin_addr, s5->alen);
    } else {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)addr;
        s5->atyp = S5ADDR_IPV6;
        s5->alen = sizeof(ipv6->sin6_addr);
        s5->port = dill_ipaddr_port(addr);
        dill_assert(s5->alen == 16);
        memcpy((void *)s5->addr, (void *)&ipv6->sin6_addr, s5->alen);
    }
}

static int _s5_handle_connection_response(int s, int64_t deadline) {
    // ensure socket is bytesstream
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) return -1;
    // largest possible connect request = 255 chars for name + 7 bytes for
    // VER, CMD, RSV, ATYP, ALEN, PORT[2]
    uint8_t conn[262];
    // reply has the same form/length limit, reuse request as reply buffer
    // read first 4 octets (through ATYP) to determine how much more to read
    int err = dill_brecv(s, conn, 4, deadline);
    if(dill_slow(err)) return -1;
    switch (conn[3]) {
        case S5ADDR_IPV4:
            // IPv4, read 4 byte IP + 2 bytes port
            err = dill_brecv(s, conn + 4, 6, deadline);
            if(dill_slow(err)) return -1;
            break;
        case S5ADDR_IPV6:
            // IPv6, read 16 bytes IP + 2 bytes port
            err = dill_brecv(s, conn + 4, 18, deadline);
            if(dill_slow(err)) return -1;
            break;
        case S5ADDR_NAME:
            // Domain name, first byte is length of string
            err = dill_brecv(s, conn + 4, 1, deadline);
            if(dill_slow(err)) return -1;
            // Read string + 2 bytes for port
            err = dill_brecv(s, conn + 5, conn[4] + 2, deadline);
            if(dill_slow(err)) return -1;
            break;
        default:
            errno = EPROTO;
            return -1;
    }
    switch (conn[1]) {
        case 0x00: // Success!
            return 0;
        case 0x01: // general SOCKS server failure
            errno = EIO; break;
        case 0x02: // connection not allowed by ruleset
            errno = EACCES; break;
        case 0x03: // Network unreachable
            errno = ENETUNREACH; break;
        case 0x04: // Host unreachable
            errno = EHOSTUNREACH; break;
        case 0x05: // Connection Refused
            errno = ECONNREFUSED; break;
        case 0x06: // TTL expired
            errno = ETIMEDOUT; break;
        case 0x07: // Command not supported
            errno = EOPNOTSUPP; break;
        case 0x08: // Address type not supported
            errno = EAFNOSUPPORT; break;
        default: // Unknown error code
            errno = EPROTO;
    }
    return -1;
}

static int dill_socks5_connectbyname(int s, const char *hostname, int port, int64_t deadline) {
    // ensure socket is bytesstream
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) return -1;
    // validate input
    if(dill_slow(!hostname)) {errno = EINVAL; return -1;}
    if(dill_slow(strlen(hostname) > 255)) {errno = EINVAL; return -1;}
    if((port <= 0) || (port>65535)) {errno = EINVAL; return -1;}
    // largest possible connect request = 255 chars for name + 7 bytes for
    // VER, CMD, RSV, ATYP, ALEN, PORT[2]
    uint8_t conn[262];
    size_t conn_len;
    conn[0] = 0x05; // VER = 5
    conn[1] = 0x01; // CMD = CONNECT
    conn[2] = 0x00; // RSV
    struct in_addr ina4;
    struct in6_addr ina6;
    // if "hostname" is actually an IPV4/IPV6 literal, convert for proxy
    if(inet_pton(AF_INET, hostname, &ina4) == 1) {
        // IPv4 Address, now stored in ina4, Network Byte Order
        conn[3] = S5ADDR_IPV4;
        memcpy((void *)(conn + 4), (void *)&ina4, 4);
        conn[8] = port >> 8;
        conn[9] = port & 0xFF;
        conn_len = 10;
    } else if(inet_pton(AF_INET6, hostname, &ina6) == 1) {
        // IPv6 Address, now stored in ina6, Network Byte Order
        conn[3] = S5ADDR_IPV6;
        memcpy((void *)(conn + 4), (void *)&ina6, 16);
        conn[20] = port >> 8;
        conn[21] = port & 0xFF;
        conn_len = 22;
   } else {
        // assume a resolvable domain name
        // previously validate to be 0 < x <= 255 bytes
        uint8_t alen = (uint8_t)strlen(hostname);
        conn[3] = S5ADDR_NAME;
        conn[4] = alen;
        memcpy((void *)(conn+ 5), (void *)hostname, alen);
        conn[5+alen] = port >> 8;
        conn[6+alen] = port & 0xFF;
        conn_len = 7 + alen;
    }
    int err = dill_bsend(s, conn, conn_len, deadline);
    if(dill_slow(err)) return -1;
    return _s5_handle_connection_response(s, deadline);
}

static int dill_socks5_connect(int s, struct dill_ipaddr *ipaddr, int64_t deadline) {
    // ensure socket is bytesstream
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) return -1;
    // validate input
    if(dill_slow(!ipaddr)) {errno = EINVAL; return -1;}
    // largest possible connect request = 255 chars for name + 7 bytes for
    // VER, CMD, RSV, ATYP, ALEN, PORT[2]
    uint8_t conn[262];
    conn[0] = 0x05; // VER = 5
    conn[1] = 0x01; // CMD = CONNECT
    conn[2] = 0x00; // RSV
    _socks5_addr s5addr;
    _s5_ipaddr_to_s5_addr(&s5addr, ipaddr);
    conn[3] = s5addr.atyp; // ATYP;
    int alen = s5addr.alen;
    memcpy((void *)&conn[4], (void *)s5addr.addr, alen);
    conn[4+alen] = s5addr.port >> 8;
    conn[5+alen] = s5addr.port & 0xFF;
    int err = dill_bsend(s, conn, alen+6, deadline);
    if(dill_slow(err)) return -1;
    return _s5_handle_connection_response(s, deadline);
}

int dill_socks5_client_connect(int s, const char *username,
      const char *password, struct dill_ipaddr *addr, int64_t deadline) {
    int err = dill_socks5_clientauth(s, username, password, deadline);
    if(err < 0) return -1;
    err = dill_socks5_connect(s, addr, deadline);
    if(err != 0) return -1;
    return 0;
}

int dill_socks5_client_connectbyname(int s, const char *username,
      const char *password, const char *hostname, int port,
      int64_t deadline) {
    int err = dill_socks5_clientauth(s, username, password, deadline);
    if(err < 0) return -1;
    err = dill_socks5_connectbyname(s, hostname, port, deadline);
    if(err != 0) return -1;
    return 0;
}
