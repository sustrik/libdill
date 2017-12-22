# NAME

ipaddr_family - returns family of the IP address

# SYNOPSIS

**int ipaddr_family(const struct ipaddr **\*_addr_**);**

# DESCRIPTION

Returns family of the address, i.e. either AF_INET (IPv4) or AF_INET6 (IPv6).

# RETURN VALUE

Family of _addr_.

# ERRORS

No errors.

# EXAMPLE

```c
int family = ipaddr_family(&addr);
```

