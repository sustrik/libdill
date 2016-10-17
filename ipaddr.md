# ipaddr(3) manual page

## NAME

ipaddr - IP address resolution

## SYNOPSIS

```c
#include <dsock.h>
typedef ipaddr;
int ipaddr_local(ipaddr *addr, const char *name, int port, int mode);
int ipaddr_remote(ipaddr *addr, const char *name, int port, int mode,
    int64_t deadline);
void ipaddr_setport(ipaddr *addr, int port);
int ipaddr_family(const ipaddr *addr);
const struct sockaddr *ipaddr_sockaddr(const ipaddr *addr);
int ipaddr_len(const ipaddr *addr);
int ipaddr_port(const ipaddr *addr);
const char *ipaddr_str(const ipaddr *addr, char *ipstr);
```

## DESCRIPTION

### Creating an address

Typedef `ipaddr` is a simple structure that can hold either IPv4 address or IPv6 address.

`ipaddr_local()` takes a name of a local network interface and returns its IP address. Setting `name` to `NULL` results in wildcard address (`IPADDR_ANY` or `in6addr_any`) which can be used to bind to all local interfaces.

`ipaddr_remote()` takes a name of a remote host and returns its IP address. To do so it may do a DNS query. `deadline` parameter can be used to terminate DNS queries that are taking substantial amount of time.

Both functions have `mode` parameter which can take following values:

* `IPADDR_IPV4`: get IPv4 address or error if it's not available
* `IPADDR_IPV6`: get IPv6 address or error if it's not available
* `IPADDR_PREF_IPV4`: get IPv4 address if possible, IPv6 address otherwise; error if none is available
* `IPADDR_PREF_IPV6`: get IPv6 address if possible, IPv4 address otherwise; error if none is available

Setting the argument to zero invokes default behaviour, which, at the present, is `IPADDR_PREF_IPV4`. However, in the future when IPv6 becomes more common it may be switched to `IPADDR_PREF_IPV6`.

Both functions return zero in case of success or -1 in case of error. In latter case `errno` is set to the appropriate error code.

`ipaddr_setport()` sets the port number in existing address, leaving the address itself intact.

### Accessing the address

`ipaddr_family()` returs the type of the address, either `AF_INET` or `AF_INET6`.

Several POSIX functions require the IP address to be passed as a pointer to `struct sockaddr` and a length of the strucuture in bytes. These values can be obtained using `ipaddr_sockaddr()` add `ipaddr_len()` functios.

`ipaddr_port()` extracts the port number from an address.

### Formatting the address

`ipaddr_str()`formats ipaddr as a human-readable string. First argument is the IP address to format, second is the buffer to store the result. The buffer must be at least `IPADDR_MAXSTRLEN` bytes long.

The function returns pointer to the formatted string.

## EXAMPLE

Resolve and print out IP address of a local network interface:

```
ipaddr addr;
int rc = ipaddr_local(&addr, "eth0", 5555, IPADDR_IPV4);
assert(rc == 0);
char buf[IPADDR_MAXSTRLEN];
printf("IP address of interface eth0 is %s.\n", ipaddr_str(&addr, buf));
```

Resolve and print out IP address of a remote host:

```
ipaddr addr;
int rc = ipaddr_remote(&addr, "www.example.org", 80, 0, now() + 1000);
assert(rc == 0);
char buf[IPADDR_MAXSTRLEN];
printf("IP address of www.example.org is %s.\n", ipaddr_str(&addr, buf));
```

Pass `ipaddr` to a POSIX function:

```
int s = socket(ipaddr_family(&addr), SOCK_STREAM, 0);
assert(s >= 0);
int rc = connect(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
assert(rc == 0);
```

