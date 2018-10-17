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

dill_unique_id(dill_socks5_type);

static void *dill_socks5_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_socks5_hclose(struct dill_hvfs *hvfs);
#if 0
static int dill_socks5_bsendl(struct dill_bsock_vfs *bvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
static int dill_socks5_brecvl(struct dill_bsock_vfs *bvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
#endif

// SOCKS5 as defined in RFC 1928

struct dill_socks5_sock {
    struct dill_hvfs hvfs;
#if 0
    struct dill_bsock_vfs bvfs;
#endif
    int u;
    unsigned int mem : 1;
    unsigned int server : 1;
    unsigned int connect : 1;
    //struct dill_suffix_storage suffix_mem;
    //struct dill_term_storage term_mem;
    char rxbuf[1024];
};

DILL_CHECK_STORAGE(dill_socks5_sock, dill_socks5_storage)

static void *dill_socks5_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_socks5_sock *obj = (struct dill_socks5_sock*)hvfs;
    if(type == dill_socks5_type) return obj;
#if 0
    if(type == dill_bsock_type) return &obj->bvfs;
#endif
    errno = ENOTSUP;
    return NULL;
}

static int dill_socks5_clientauth(int s, const char *username, const char *password,
      int64_t deadline) {
    // ensure socket is attached as socks5 
    struct dill_socks5_sock *obj = dill_hquery(s, dill_socks5_type);
    if(dill_slow(!obj)) return -1;
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
    int err = dill_bsend(obj->u, (void *)vims, vims_len, deadline);
    if(dill_slow(err)) {return -1;}
    // version identifier/method response
    // server responds with chosen auth method (or error)
    uint8_t vimr[2];
    err = dill_brecv(obj->u, (void *)vimr, 2, deadline);
    if(dill_slow(err)) {return -1;}
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
            err = dill_bsend(obj->u, (void *)upauth, 3 + ulen + plen, deadline);
            if(dill_slow(err)) {return -1;}
            uint8_t upauthr[2];
            err = dill_brecv(obj->u, (void *)upauthr, 2, deadline);
            if(dill_slow(err)) {return -1;}
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

int dill_socks5_attach_client_mem(int s, const char *username,
      const char *password, struct dill_socks5_storage *mem,
      int64_t deadline) {
    int err;
    struct dill_socks5_sock *obj = (struct dill_socks5_sock*)mem;
    if(dill_slow(!mem)) {err = EINVAL; goto error;}
    /* Check whether underlying socket is a bytestream. */
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) {err = errno; goto error;}
    /* Take the ownership of the underlying socket. */
    s = dill_hown(s);
    if(dill_slow(s < 0)) {err = errno; goto error;}
    /* Create the object. */
    obj->hvfs.query = dill_socks5_hquery;
    obj->hvfs.close = dill_socks5_hclose;
#if 0
    obj->bvfs.bsendl = dill_socks5_bsendl;
    obj->bvfs.brecvl = dill_socks5_brecvl;
#endif
    obj->u = s;
    obj->mem = 1;
    obj->server = 0;
    obj->connect = 0;
    /* Create the handle. */
    int h = dill_hmake(&obj->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error;}
    err = dill_socks5_clientauth(h, username, password, deadline);
    if(err) {s = h; err = errno; goto error;}
    return h;
error:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_socks5_attach_client(int s, const char *username,
      const char *password, int64_t deadline) {
    int err;
    struct dill_socks5_sock *obj = malloc(sizeof(struct dill_socks5_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    s = dill_socks5_attach_client_mem(s, username, password,
        (struct dill_socks5_storage*)obj, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    if(s >= 0) dill_hclose(s);
    errno = err;
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
    struct dill_socks5_sock *obj = dill_hquery(s, dill_socks5_type);
    // largest possible connect request = 255 chars for name + 7 bytes for
    // VER, CMD, RSV, ATYP, ALEN, PORT[2]
    uint8_t conn[262];
    // reply has the same form/length limit, reuse request as reply buffer
    // read first 4 octets (through ATYP) to determine how much more to read
    int err = dill_brecv(obj->u, conn, 4, deadline);
    if(dill_slow(err)) {return -1;}
    switch (conn[3]) {
        case S5ADDR_IPV4:
            // IPv4, read 4 byte IP + 2 bytes port
            err = dill_brecv(obj->u, conn + 4, 6, deadline);
            if(dill_slow(err)) {return -1;}
            break;
        case S5ADDR_IPV6:
            // IPv6, read 16 bytes IP + 2 bytes port
            err = dill_brecv(obj->u, conn + 4, 18, deadline);
            if(dill_slow(err)) {return -1;}
            break;
        case S5ADDR_NAME:
            // Domain name, first byte is length of string
            err = dill_brecv(obj->u, conn + 4, 1, deadline);
            if(dill_slow(err)) {return -1;}
            // Read string + 2 bytes for port
            err = dill_brecv(obj->u, conn + 5, conn[4] + 2, deadline);
            if(dill_slow(err)) {return -1;}
            break;
        default:
            errno = EPROTO;
            return -1;
    }
    if(conn[1] == 0x00) {
        // success - from here on should treat as if attached via tcp_connect
        obj->connect = 1;
        return 0;
    }
    switch (conn[1]) {
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

int dill_socks5_connectbyname(int s, char *hostname, int port, int64_t deadline) {
    // validate socket type
    struct dill_socks5_sock *obj = dill_hquery(s, dill_socks5_type);
    if(dill_slow(!obj)) return -1;
    if(dill_slow(obj->server)) {errno = EPROTO; return -1;}
    if(dill_slow(obj->connect)) {errno = EISCONN; return -1;}
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
        conn[3] = 0x01; // ATYP = IPV4
        memcpy((void *)(conn + 4), (void *)&ina4, 4);
        conn[8] = port >> 8;
        conn[9] = port & 0xFF;
        conn_len = 10;
    } else if(inet_pton(AF_INET6, hostname, &ina6) == 1) {
        // IPv6 Address, now stored in ina6, Network Byte Order
        conn[3] = 0x04; // ATYP = IPV6
        memcpy((void *)(conn + 4), (void *)&ina6, 16);
        conn[20] = port >> 8;
        conn[21] = port & 0xFF;
        conn_len = 22;
   } else {
        // assume a resolvable domain name
        // previously validate to be 0 < x <= 255 bytes
        uint8_t alen = (uint8_t)strlen(hostname);
        conn[3] = 0x03; // ATYP = DOMAINNAME
        conn[4] = alen;
        memcpy((void *)(conn+ 5), (void *)hostname, alen);
        conn[5+alen] = port >> 8;
        conn[6+alen] = port & 0xFF;
        conn_len = 7 + alen;
    }
    int err = dill_bsend(obj->u, conn, conn_len, deadline);
    if(dill_slow(err)) {return -1;}
    return _s5_handle_connection_response(s, deadline);
}

int dill_socks5_connect(int s, struct dill_ipaddr *ipaddr, int64_t deadline) {
    // validate socket type
    struct dill_socks5_sock *obj = dill_hquery(s, dill_socks5_type);
    if(dill_slow(!obj)) return -1;
    if(dill_slow(obj->server)) {errno = EPROTO; return -1;}
    if(dill_slow(obj->connect)) {errno = EISCONN; return -1;}
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
    int err = dill_bsend(obj->u, conn, alen+6, deadline);
    if(dill_slow(err)) {return -1;}
    return _s5_handle_connection_response(s, deadline);
}

int dill_socks5_detach(int s, int64_t deadline) {
    struct dill_socks5_sock *obj = dill_hquery(s, dill_socks5_type);
    if(dill_slow(!obj)) return -1;
    int u = obj->u;
    if(!obj->mem) free(obj);
    return u;
}

static void dill_socks5_hclose(struct dill_hvfs *hvfs) {
    struct dill_socks5_sock *obj = (struct dill_socks5_sock*)hvfs;
    if(dill_fast(obj->u >= 0)) {
        int rc = dill_hclose(obj->u);
        dill_assert(rc == 0);
    }
    if(!obj->mem) free(obj);
}

#if 0
// if connected, pass send through to underlying transport
static int dill_socks5_bsendl(struct dill_bsock_vfs *bvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_socks5_sock *obj = dill_cont(bvfs, struct dill_socks5_sock, bvfs);
    if(dill_slow(!obj->connect)) {errno = ENOENT; return -1;}
    return dill_bsendl(obj->u, first, last, deadline);
}

// if connected, pass recv through to underlying transport
static int dill_socks5_brecvl(struct dill_bsock_vfs *bvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_socks5_sock *obj = dill_cont(bvfs, struct dill_socks5_sock, bvfs);
    if(dill_slow(!obj->connect)) {errno = ENOENT; return -1;}
    return dill_brecvl(obj->u, first, last, deadline);
}
#endif