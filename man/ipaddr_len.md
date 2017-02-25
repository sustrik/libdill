# NAME

ipaddr_len - returns length of the address

# SYNOPSIS

**int ipaddr_len(const struct ipaddr **\*_addr_**);**

# DESCRIPTION

Returns lenght of the address, in bytes. This function is typically used in combination with ipaddr_sockaddr to pass address and its length to POSIX socket APIs.

# RETURN VALUE

Length of the address in _addr_.

# ERRORS

No errors.

# EXAMPLE

```c
int rc = connect(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
```

