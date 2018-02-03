# NAME

ipaddr_local - resolve a name of a local network interface

# SYNOPSIS

**int ipaddr_remote(struct ipaddr **\*_addr_**, const char **\*_name_**, int** _port_**, int** _mode_**, int64_t** _deadline_**);**

# DESCRIPTION

Converts an IP address in human-readable format, or a name of a remote host into an ipaddr structure:

```
ipaddr_remote(&addr, "192.168.0.111", 80, 0, -1);
ipaddr_remote(&addr, "www.expamle.org", 443, 0, -1);
```

_addr_ is thr address strucure to hold the result. _name_ is the string to convert. _port_ is the port number to use. _mode_ specifies which kind of addresses should be returned. Possible values are:

* **IPADDR_IPV4**: Get IPv4 address.
* **IPADDR_IPV6**: Get IPv6 address.
* **IPADDR_PREF_IPV4**: Get IPv4 address if possible, IPv6 address otherwise.
* **IPADDR_PREF_IPV6**: Get IPv6 address if possible, IPv4 address otherwise.

Setting the argument to zero invokes default behaviour, which, at the present, is IPADDR_PREF_IPV4. However, in the future when IPv6 becomes more common it may be switched to IPADDR_PREF_IPV6.

Finally, the last argument is the deadline. It allows to deal with situations where resolving a remote host name requires a DNS query and the query is taking substantial amount of time to complete.

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EADDRNOTAVAIL**: The name of the remote host cannot be resolved to an address of the specified type.

# EXAMPLE

```c
struct ipaddr addr;
int rc = ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
```

