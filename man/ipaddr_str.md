# NAME

ipaddr_str - convert address to a human-readable string

# SYNOPSIS

```c
#include <libdill.h>

const char* ipaddr_str(const struct ipaddr* addr, char* buf);
```

# DESCRIPTION

Formats address as a human-readable string.

**addr**: IP address object.

**buf**: Buffer to store the result in. It must be at least **IPADDR_MAXSTRLEN** bytes long.

# RETURN VALUE

The function returns **ipstr** argument, i.e.  pointer to the formatted string.

# ERRORS

None.

# EXAMPLE

```c
char buf[IPADDR_MAXSTRLEN];
ipaddr_str(&addr, buf);
```
