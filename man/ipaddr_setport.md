# NAME

ipaddr_setport - changes port number of the address

# SYNOPSIS

```c
#include <libdill.h>

void ipaddr_setport(const struct ipaddr* addr);
```

# DESCRIPTION

Changes port number of the address.

**addr**: IP address object.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

None.

# ERRORS

None.

# EXAMPLE

```c
ipaddr_setport(&addr, 80);
```
# SEE ALSO

**ipaddr_family**(3) **ipaddr_len**(3) **ipaddr_local**(3) **ipaddr_port**(3) **ipaddr_remote**(3) **ipaddr_sockaddr**(3) **ipaddr_str**(3) 
