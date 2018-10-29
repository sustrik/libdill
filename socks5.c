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
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "utils.h"

#define RFC1928_VER (0x05)
#define S5METH_NOAUTHREQ (0x00)
#define S5METH_USERPASS (0x02)
#define S5METH_NOMETHOD (0xFF) // failure case

#define RFC1929_VER (0x01)
#define S5AUTH_SUCCESS (0x00)
#define S5AUTH_FAIL (0xFF) // actually any nonzero value denotes fail

#define S5ADDR_IPV4 (1)
#define S5ADDR_IPV6 (4)
#define S5ADDR_NAME (3)

#define S5ADDR_IPV4_SZ (4)
#define S5ADDR_IPV6_SZ (16)

typedef struct {
    uint8_t atyp;
    uint8_t addr[257];
    size_t  alen;
    int     port;
} _socks5_addr;

#define S5REPLY_MIN (DILL_SOCKS5_SUCCESS)
#define S5REPLY_MAX (DILL_SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED)

static int s5_client_auth(int s, const char *username, const char *password,
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
    vims[0] = RFC1928_VER;
    vims[2] = S5METH_NOAUTHREQ;
    vims[3] = S5METH_USERPASS;
    // TODO: Add support for GSSAPI Auth
    // if we don't have both username and password only request NO AUTH
    if((!username) || (!password)) {
        vims[1] = 1; // NMETHODS = 1 (NO AUTH)
        vims_len = 3;
    } else {
        vims[1] = 2; // NMETHODS = 2 (NO AUTH or USER/PASS)
        vims_len = 4;
    }
    int err = dill_bsend(s, (void *)vims, vims_len, deadline);
    if(dill_slow(err)) return -1;
    // version identifier/method response
    // server responds with chosen auth method (or error)
    uint8_t vimr[2];
    err = dill_brecv(s, (void *)vimr, 2, deadline);
    if(dill_slow(err)) return -1;
    // validate VER in response
    if(dill_slow(vimr[0] != RFC1928_VER)) {errno = EPROTO; return -1;}
    switch(vimr[1]) {
        case S5METH_NOAUTHREQ:
            // proxy accepted NO AUTH REQ'D, so we're done
            return 0;
        case S5METH_USERPASS:
            dill_assert(!!username);
            dill_assert(!!password);
            // USER/PASS AUTH - per RFC 1929
            // max USER, PASS is 255 bytes (plus 3 bytes for VER, ULEN, PLEN)
            uint8_t upauth[513];
            // previously range checked, safe to cast to uint8_t
            uint8_t ulen = (uint8_t)strlen(username);
            uint8_t plen = (uint8_t)strlen(password);
            upauth[0] = RFC1929_VER; // VER = 1
            upauth[1] = ulen; // ULEN
            strcpy((char *)(upauth + 2), username); // UNAME
            upauth[2 + ulen] = plen; // PLEN
            strcpy((char *)(upauth + 3 + ulen), password); // PASSWD
            err = dill_bsend(s, (void *)upauth, 3 + ulen + plen, deadline);
            if(dill_slow(err)) return -1;
            uint8_t upauthr[2];
            err = dill_brecv(s, (void *)upauthr, 2, deadline);
            if(dill_slow(err)) return -1;
            if(dill_slow(upauthr[0] != RFC1929_VER)) {errno = EPROTO; return -1;}
            if(dill_slow(upauthr[1] != S5AUTH_SUCCESS)) {errno = EACCES; return -1;}
            return 0;
        case 0xFF:
            errno = EACCES; return -1;
        default:
            // something is fishy as the proxy accepted something not requested
            dill_assert(0);
    }
    return -1;
}

int dill_socks5_proxy_auth(int s, dill_socks5_auth_function *auth_fn,
      int64_t deadline) {
    // all responses are 2 octets
    uint8_t resp[2];
    // ensure socket is bytesstream
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) return -1;
    // max version identification/message selection message is 257 octets
    uint8_t vims[257];
    // read first 2 octets of vims request (2nd byte is length)
    int err = dill_brecv(s, (void *)vims, 2, deadline);
    if(dill_slow(err)) return -1;
    // validate VER
    if(dill_slow(vims[0] != RFC1928_VER)) {errno = EPROTO; return -1;}
    err = dill_brecv(s, (void *)&(vims[2]), vims[1], deadline);
    if(dill_slow(err)) return -1;
    int req_up = 0;
    int req_na = 0;
    for (int i = 0; i < vims[1]; i++) {
        switch(vims[2+i]) {
            case S5METH_NOAUTHREQ:
                req_na = 1; break;
            case S5METH_USERPASS:
                req_up = 1; break;
            //default:
                // ignore methods which aren't supported by this implementation
        }
    }
    if(req_na) {
        if(!auth_fn) {
            goto accept_noauth;
        }
    }
    if((req_up) && (auth_fn)) {
        resp[0] = RFC1928_VER;
        resp[1] = S5METH_USERPASS;
        err = dill_bsend(s, (void *)resp, 2, deadline);
        if(dill_slow(err)) return -1;
        // max size 513 (plus 1 for terminating null)
        uint8_t authr[514];
        // read 2 octets of RFC 1929 auth request, VER, ULEN
        err = dill_brecv(s, (void *)authr, 2, deadline);
        if(dill_slow(err)) return -1;
        // VER for U/P auth is 0x01
        if(dill_slow(authr[0] != RFC1929_VER)) {errno = EPROTO; return -1;}
        int ulen = authr[1];
        // read UNAME
        err = dill_brecv(s, (void *)&(authr[2]), ulen, deadline);
        if(dill_slow(err)) return -1;
        // read PLEN
        err = dill_brecv(s, (void *)&(authr[2+ulen]), 1, deadline);
        if(dill_slow(err)) return -1;
        int plen = authr[2+ulen];
        // read PASSWD
        err = dill_brecv(s, (void *)&(authr[3+ulen]), plen, deadline);
        if(dill_slow(err)) return -1;
        // add null terminators in place
        authr[2+ulen] = 0x00;
        authr[3+ulen+plen] = 0x00;
        char *uname = (char *)&authr[2];
        char *passwd = (char *)&authr[3+ulen];
        // attempt to determine source address
        if((*auth_fn)(uname, passwd)) {
            // auth accepted based on uname, passwd, ipaddr
            goto pwauth_success;
        }
        // PW auth fail
        resp[0] = RFC1929_VER; // VER
        resp[1] = S5AUTH_FAIL;
        err = dill_bsend(s, (void *)resp, 2, deadline);
        if(dill_slow(err)) return -1;
        return -1;
    }
    // no compatible AUTH (auth fail)
    resp[0] = RFC1928_VER; // VER
    resp[1] = S5METH_NOMETHOD;
    err = dill_bsend(s, (void *)resp, 2, deadline);
    if(dill_slow(err)) return -1;
    return -1;
accept_noauth:
    // accept NOAUTH, no further auth req'd
    resp[0] = RFC1928_VER; // VER
    resp[1] = S5METH_NOAUTHREQ;
    err = dill_bsend(s, (void *)resp, 2, deadline);
    if(dill_slow(err)) return -1;
    return 0;
pwauth_success:
    // accept NOAUTH, no further auth req'd
    resp[0] = RFC1929_VER; // U/P auth VER
    resp[1] = S5AUTH_SUCCESS;
    err = dill_bsend(s, (void *)resp, 2, deadline);
    if(dill_slow(err)) return -1;
    return 0;
}

// convenience functions for handling socks5 address types

static int s5_ipaddr_to_s5addr(_socks5_addr *s5, struct dill_ipaddr *addr) {
    switch(dill_ipaddr_family(addr)) {
        case AF_INET: {
            struct sockaddr_in *ipv4 = (struct sockaddr_in*)addr;
            s5->atyp = S5ADDR_IPV4;
            s5->alen = sizeof(ipv4->sin_addr);
            s5->port = dill_ipaddr_port(addr);
            dill_assert(s5->alen == 4);
            memcpy((void *)s5->addr, (void *)&ipv4->sin_addr, s5->alen);
            return 0;}
        case AF_INET6: {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)addr;
            s5->atyp = S5ADDR_IPV6;
            s5->alen = sizeof(ipv6->sin6_addr);
            s5->port = dill_ipaddr_port(addr);
            dill_assert(s5->alen == 16);
            memcpy((void *)s5->addr, (void *)&ipv6->sin6_addr, s5->alen);
            return 0;}
    }
    return -1;
}

static int s5_s5addr_to_ipaddr(struct dill_ipaddr *addr, _socks5_addr *s5,
      int64_t deadline) {
    int err;
    if((s5->port < 0) || (s5->port > 65535)) {errno = EINVAL; return -1;}
    switch(s5->atyp){
        case S5ADDR_IPV4:
            // create with generic IPV4
            err = dill_ipaddr_remote(addr, "127.0.0.1", s5->port,
                DILL_IPADDR_IPV4, deadline);
            if(err) return -1;
            // overwrite with actual IPV4
            struct sockaddr_in *ipv4 = (struct sockaddr_in*)addr;
            memcpy((void *)&ipv4->sin_addr, (void *)s5->addr, s5->alen);
            break;
        case S5ADDR_IPV6:
            // create with generic IPV6
            err = dill_ipaddr_remote(addr, "::1", s5->port,
                DILL_IPADDR_IPV6, deadline);
            if(err) return -1;
            // overwrite with actual IPV4
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)addr;
            memcpy((void *)&ipv6->sin6_addr, (void *)s5->addr, s5->alen);
            break;
        case S5ADDR_NAME:
            // It's a name... look it up.
            // addr is sized to always have room for a null terminator
            err = dill_ipaddr_remote(addr, (const char*)&(s5->addr[0]),
                s5->port, DILL_IPADDR_PREF_IPV4, deadline);
            if(err) return -1;
        break;
    }
    return 0;
}

// command request and response packets have the same
// variable length structure (and max length = 262)
static int s5_recv_command_request_response(int s, uint8_t *conn, int64_t deadline) {
    int err = dill_brecv(s, conn, 4, deadline);
    if(dill_slow(err)) return -1;
    if(conn[0] != RFC1928_VER) {errno = EPROTO; return -1;} // VER
    // don't validate CMD/REP field, range depends on CMD or REP
    if(conn[2] != 0x00) {errno = EPROTO; return -1;} // RSV
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
    return 0;
}

static int s5_handle_connection_response(int s, int64_t deadline) {
    // ensure socket is bytesstream
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) return -1;
    // largest possible connect response = 255 chars for name + 7 bytes for
    // VER, CMD, RSV, ATYP, ALEN, PORT[2]
    uint8_t conn[262];
    int err = s5_recv_command_request_response(s, conn, deadline);
    if(err) return -1;
    // validate REP
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

static int s5_client_connectbyname(int s, const char *hostname, int port, int64_t deadline) {
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
    conn[0] = RFC1928_VER; // VER
    conn[1] = DILL_SOCKS5_CONNECT; // CMD
    conn[2] = 0x00; // RSV
    struct in_addr ina4;
    struct in6_addr ina6;
    // if "hostname" is actually an IPV4/IPV6 literal, convert for proxy
    if(inet_pton(AF_INET, hostname, &ina4) == 1) {
        // IPv4 Address, now stored in ina4, Network Byte Order
        conn[3] = S5ADDR_IPV4; // ATYP
        memcpy((void *)(conn + 4), (void *)&ina4, 4);
        conn[8] = port >> 8;
        conn[9] = port & 0xFF;
        conn_len = 10;
    } else if(inet_pton(AF_INET6, hostname, &ina6) == 1) {
        // IPv6 Address, now stored in ina6, Network Byte Order
        conn[3] = S5ADDR_IPV6; // ATYP
        memcpy((void *)(conn + 4), (void *)&ina6, 16);
        conn[20] = port >> 8;
        conn[21] = port & 0xFF;
        conn_len = 22;
   } else {
        // assume a resolvable domain name
        // previously validate to be 0 < x <= 255 bytes
        uint8_t alen = (uint8_t)strlen(hostname);
        conn[3] = S5ADDR_NAME; // ATYP
        conn[4] = alen;
        memcpy((void *)(conn+ 5), (void *)hostname, alen);
        conn[5+alen] = port >> 8;
        conn[6+alen] = port & 0xFF;
        conn_len = 7 + alen;
    }
    int err = dill_bsend(s, conn, conn_len, deadline);
    if(dill_slow(err)) return -1;
    return s5_handle_connection_response(s, deadline);
}

static int s5_client_connect(int s, struct dill_ipaddr *ipaddr, int64_t deadline) {
    // ensure socket is bytesstream
    if(dill_slow(!dill_hquery(s, dill_bsock_type))) return -1;
    // validate input
    if(dill_slow(!ipaddr)) {errno = EINVAL; return -1;}
    // largest possible connect request = 255 chars for name + 7 bytes for
    // VER, CMD, RSV, ATYP, ALEN, PORT[2]
    uint8_t conn[262];
    conn[0] = RFC1928_VER; // VER
    conn[1] = DILL_SOCKS5_CONNECT; // CMD
    conn[2] = 0x00; // RSV
    _socks5_addr s5addr;
    int err = s5_ipaddr_to_s5addr(&s5addr, ipaddr);
    if(dill_slow(err)) return -1;
    conn[3] = s5addr.atyp; // ATYP;
    int alen = s5addr.alen;
    memcpy((void *)&conn[4], (void *)s5addr.addr, alen);
    conn[4+alen] = s5addr.port >> 8;
    conn[5+alen] = s5addr.port & 0xFF;
    err = dill_bsend(s, conn, alen+6, deadline);
    if(dill_slow(err)) return -1;
    return s5_handle_connection_response(s, deadline);
}

int dill_socks5_proxy_recvcommand(int s, struct dill_ipaddr *ipaddr,
      int64_t deadline) {
    // largest possible connect request = 255 chars for name + 7 bytes for
    // VER, CMD, RSV, ATYP, ALEN, PORT[2]
    uint8_t conn[262];
    int err = s5_recv_command_request_response(s, conn, deadline);
    if(err) return -1;
    if((conn[1] < DILL_SOCKS5_CONNECT) || (conn[1] > DILL_SOCKS5_UDP_ASSOCIATE)) {
        errno = EPROTO ; return -1;
    }
    _socks5_addr s5addr;
    s5addr.atyp = conn[3];
    uint16_t *port;
    switch(s5addr.atyp) {
        case S5ADDR_IPV4:
            memcpy((void *)&(s5addr.addr), (void *)&(conn[4]), S5ADDR_IPV4_SZ);
            s5addr.alen = S5ADDR_IPV4_SZ;
            port = (uint16_t *)&(conn[4 + S5ADDR_IPV4_SZ]);
            break;
        case S5ADDR_IPV6:
            memcpy((void *)&(s5addr.addr), (void *)&(conn[4]), S5ADDR_IPV6_SZ);
            s5addr.alen = S5ADDR_IPV6_SZ;
            port = (uint16_t *)&(conn[4 + S5ADDR_IPV6_SZ]);
            break;
        case S5ADDR_NAME:
            // first octet is len
            memcpy((void *)&(s5addr.addr), (void *)&(conn[5]), conn[4]);
            // add null terminator
            s5addr.addr[conn[4]] = '\x00';
            s5addr.alen = conn[4];
            port = (uint16_t *)&(conn[4 + 1 + conn[4]]);
            break;
    }
    s5addr.port = (int)ntohs(*port);
    err = s5_s5addr_to_ipaddr(ipaddr, &s5addr, deadline);
    if(err) return -1;
    return conn[1];
}

int dill_socks5_proxy_recvcommandbyname(int s, char *host, int *port,
      int64_t deadline) {
    // largest possible connect request = 255 chars for name + 7 bytes for
    // VER, CMD, RSV, ATYP, ALEN, PORT[2]
    uint8_t conn[262];
    int err = s5_recv_command_request_response(s, conn, deadline);
    if(err) return -1;
    if((conn[1] < DILL_SOCKS5_CONNECT) || (conn[1] > DILL_SOCKS5_UDP_ASSOCIATE)) {
        errno = EPROTO ; return -1;
    }
    uint16_t *s5port;
    switch(conn[3]) {
        case S5ADDR_IPV4:
            inet_ntop(AF_INET, (void *)&(conn[4]), host, 256);
            s5port = (uint16_t *)&(conn[4 + S5ADDR_IPV4_SZ]);
            break;
        case S5ADDR_IPV6:
            inet_ntop(AF_INET6, (void *)&(conn[4]), host, 256);
            s5port = (uint16_t *)&(conn[4 + S5ADDR_IPV6_SZ]);
            break;
        case S5ADDR_NAME:
            // first octet is len
            memcpy((void *)host, (void *)&(conn[5]), conn[4]);
            // add null terminator
            host[conn[4]] = '\x00';
            s5port = (uint16_t *)&(conn[4 + 1 + conn[4]]);
            break;
    }
    *port = (int)ntohs(*s5port);
    return conn[1];
}

int dill_socks5_proxy_sendreply(int s, int reply, struct dill_ipaddr *ipaddr,
      int64_t deadline) {
    if(dill_slow((reply < S5REPLY_MIN) || (reply > S5REPLY_MAX))) {
        errno = EINVAL; return -1;
    }
    _socks5_addr s5addr;
    int err = s5_ipaddr_to_s5addr(&s5addr, ipaddr);
    if(dill_slow(err)) return -1;
    // largest possible connect request = 255 chars for name + 7 bytes for
    // VER, CMD, RSV, ATYP, ALEN, PORT[2]
    uint8_t conn[262];
    conn[0] = RFC1928_VER; // VER
    conn[1] = reply; // CMD
    conn[2] = 0x00; // RSV
    conn[3] = s5addr.atyp;
    int alen = s5addr.alen;
    switch(s5addr.atyp) {
        case S5ADDR_IPV4:
            memcpy((void *)&(conn[4]), (void *)&(s5addr.addr), S5ADDR_IPV4_SZ);
            break;
        case S5ADDR_IPV6:
            memcpy((void *)&(conn[4]), (void *)&(s5addr.addr), S5ADDR_IPV6_SZ);
            break;
        case S5ADDR_NAME:
            conn[4] = alen;
            memcpy((void *)&(conn[5]), (void *)&(s5addr.addr), alen);
            alen += 1;
            break;
        default:
            dill_assert(0);
    }
    conn[4+alen] = s5addr.port >> 8;
    conn[5+alen] = s5addr.port & 0xFF;
    err = dill_bsend(s, conn, alen+6, deadline);
    if(dill_slow(err)) return -1;
    return 0;
}

int dill_socks5_client_connect(int s, const char *username,
      const char *password, struct dill_ipaddr *addr, int64_t deadline) {
    int err = s5_client_auth(s, username, password, deadline);
    if(err < 0) return -1;
    err = s5_client_connect(s, addr, deadline);
    if(err != 0) return -1;
    return 0;
}

int dill_socks5_client_connectbyname(int s, const char *username,
      const char *password, const char *hostname, int port,
      int64_t deadline) {
    int err = s5_client_auth(s, username, password, deadline);
    if(err < 0) return -1;
    err = s5_client_connectbyname(s, hostname, port, deadline);
    if(err != 0) return -1;
    return 0;
}
