# NAME

ipaddr_port - returns port part of the address

# SYNOPSIS

**int ipaddr_port(const struct ipaddr **\*_addr_**);**

# DESCRIPTION

Returns port part of the address.

# RETURN VALUE

The port number.

# ERRORS

No errors.

# EXAMPLE

```c
int port = ipaddr_port(&addr);
```

