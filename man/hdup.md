# NAME

hdup - duplicates a handle

# SYNOPSIS

**#include &lt;libdill.h>**

**int hdup(int** _h_**);**

# DESCRIPTION

Duplicates a handle. The new handle will refer to the same underlying object.

Each duplicate of a handle requires its own call to **hclose**. The underlying object is deallocated when all handles pointing to it have been closed.

# RETURN VALUE

Returns the new handle duplicate on success. On error, -1 is returned and _errno_ is set to one of the following values.

# ERRORS

* **EBADF**: Invalid handle.

# EXAMPLE

```c
int ch1 = chmake(sizeof(int));
int ch2 = hdup(ch1);
hclose(ch2);
hclose(ch1);
```

