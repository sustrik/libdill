# NAME

ipaddr_setport - changes port number of the address

# SYNOPSIS

**void ipaddr_setport(struct ipaddr **\*_addr_**, int** _port_**);**

# DESCRIPTION

Changes port number of the address.

# RETURN VALUE

None.

# ERRORS

No errors.

# EXAMPLE

```c
ipaddr_setport(&addr, 80);
```

