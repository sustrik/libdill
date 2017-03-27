# NAME

hclose - hard-cancels a handle

# SYNOPSIS

**#include &lt;libdill.h>**

**int hclose(int** _h_**);**

# DESCRIPTION

Closes a handle.

Once all handles pointing to the same underlying object have been closed, the object is deallocated immediately.

The function guarantees that all associated resources are deallocated. However, it does
not guarantee that the handle's work will have been fully finished. E.g., outbound network data may not be flushed.

# RETURN VALUE

In the case of an error, it returns -1 and sets _errno_ to one of the values. In case of success, returns 0.

# ERRORS

* **EBADF**: Invalid handle.

# EXAMPLE

```c
int ch = chmake(sizeof(int));
hclose(ch);
```

