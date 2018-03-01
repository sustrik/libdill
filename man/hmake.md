# NAME

hmake - creates a handle

# SYNOPSIS

```c
#include <libdillimpl.h>

struct hvfs {
    void *(*query)(struct hvfs *vfs, const void *type);
    void (*close)(int h);
    int (*done)(struct hvfs *vfs, int64_t deadline);
};

int hmake(struct hvfs* hvfs);
```

# DESCRIPTION

A handle is the user-space equivalent of a file descriptor.Coroutines and channels are represented by handles.

Unlike with file descriptors, however, you can use the **hmake**function to create your own type of handle.

The argument of the function is a virtual-function table ofoperations associated with the handle.

When implementing the **close** operation, keep in mind thatinvoking blocking operations is not allowed, as blocking operationsinvoked within the context of a **close** operation will fail withan **ECANCELED** error.

To close a handle, use the **hclose** function.

**hvfs**: virtual-function table of operations associated with the handle

# RETURN VALUE

In case of success the function returns newly created handle. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine was canceled.
* **EINVAL**: Invalid argument.
* **ENOMEM**: Not enough memory.

# SEE ALSO

**hclose**(3) **hdone**(3) **hdup**(3) **hquery**(3) 

