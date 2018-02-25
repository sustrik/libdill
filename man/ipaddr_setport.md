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

# RETURN VALUE

None.

# ERRORS

None.

# EXAMPLE

```c
ipaddr_setport(&addr, 80);
```
