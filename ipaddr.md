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
const char *ipaddr_str(const ipaddr *addr, char *ipstr);
int ipaddr_family(const ipaddr *addr);
const struct sockaddr *ipaddr_sockaddr(const ipaddr *addr);
int ipaddr_len(const ipaddr *addr);
int ipaddr_port(const ipaddr *addr);
void ipaddr_setport(ipaddr *addr, int port);
```

## DESCRIPTION

TODO

## EXAMPLE

TODO

