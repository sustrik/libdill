# NAME

ipaddr_local - resolve a name of a local network interface

# SYNOPSIS

**int ipaddr_local(struct ipaddr **\*_addr_**, const char **\*_name_**, int** _port_**, int** _mode_**);**

# DESCRIPTION

Converts an IP address in human-readable format, or a name of a local network interface, into an ipaddr object:

```
ipaddr_local(&addr, "192.168.0.111", 80, 0);
ipaddr_local(&addr, "::1", 443, 0);
ipaddr_local(&addr, "eth0", 25, 0);
```
_addr_ is thr address strucure to hold the result. _name_ is the string to convert. _port_ is the port number to use. _mode_ specifies which kind of addresses should be returned. Possible values are:

* **IPADDR_IPV4**: Get IPv4 address.
* **IPADDR_IPV6**: Get IPv6 address.
* **IPADDR_PREF_IPV4**: Get IPv4 address if possible, IPv6 address otherwise.
* **IPADDR_PREF_IPV6**: Get IPv6 address if possible, IPv4 address otherwise.

Setting the argument to zero invokes default behaviour, which, at the present, is IPADDR_PREF_IPV4. However, in the future when IPv6 becomes more common it may be switched to IPADDR_PREF_IPV6.

If address parameter is set to NULL, INADDR_ANY or in6addr_any, respectively, is returned. This value is useful when binding to all local network interfaces.

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ENODEV**: Local network interface with the specified name does not exist.

# EXAMPLE

```c
struct ipaddr addr;
int rc = ipaddr_local(&addr, "eth0", 5555, 0);
```

