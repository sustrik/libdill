# NAME

hclose - hard-closes a handle

# SYNOPSIS

```c
#include <libdill.h>

int hclose(int h);
```

# DESCRIPTION

This function closes a handle. Once all handles pointing to the same
underlying object have been closed, the object is deallocated
immediately, without blocking.

The  function  guarantees that all associated resources are
deallocated. However, it does not guarantee that the handle's work
will have been fully finished. E.g., outbound network data may not
be flushed.

In the case of network protocol sockets the entire protocol stack
is closed, the topmost protocol as well as all the protocols
beneath it.

**h**: The handle.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ENOTSUP**: The handle does not support this operation.

# EXAMPLE

```c
int ch[2];
chmake(ch);
hclose(ch[0]);
hclose(ch[1]);
```
