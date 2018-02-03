# NAME

ipaddr_sockaddr - returns sockaddr structure corresponding to the IP address

# SYNOPSIS

**const struct sockaddr *ipaddr_sockaddr(const struct ipaddr **\*_addr_**);**

# DESCRIPTION

Returns sockaddr structure corresponding to the IP address. This function is typically used in combination with ipaddr_len to pass address and its length to POSIX socket APIs.

# RETURN VALUE

Pointer to sockaddr structure correspoding the address in _addr_.

# ERRORS

No errors.

# EXAMPLE

```c
int rc = connect(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
```

