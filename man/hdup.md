# NAME

hdup - duplicates a handle

# SYNOPSIS

```c
#include <libdill.h>

int hdup(int h);
```

# DESCRIPTION

Duplicates a handle. The new handle will refer to the same
underlying object.

**h**: Handle to duplicate.

Each duplicate of a handle requires its own call to **hclose**.
The underlying object is deallocated when all handles pointing to it
have been closed.

# RETURN VALUE

In case of success the function returns newly duplicated handle. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.

# EXAMPLE

```c
int h1 = tcp_connect(&addr, deadline);
h2 = hdup(h1);
hclose(h1);
hclose(h2); /* The socket gets deallocated here. */
```
# SEE ALSO

**hclose**(3) **hclose**(3) **hdone**(3) **hmake**(3) **hquery**(3) 
