# NAME

ipaddr_str - convert address to a human-readable string

# SYNOPSIS

**const char *ipaddr_str(const struct ipaddr **\*_addr_**, char **\*_ipstr_**);**

# DESCRIPTION

Formats address as a human-readable string.

_addr_ is the address to convert. _ipstr_ is the buffer to store the result it. The buffer must be at least IPADDR_MAXSTRLEN bytes long.

# RETURN VALUE

The function returns _ipstr_ argument, i.e. pointer to the formatted string.

# ERRORS

No errors.

# EXAMPLE

```c
char buf[IPADDR_MAXSTRLEN];
ipaddr_str(&addr, buf);
```

