# NAME

hmake - creates a handle

# SYNOPSIS

```c
struct hvfs {
    void *(*query)(struct hvfs *vfs, const void *type);
    void (*close)(int h);
};
```

**#include **&lt;libdill.h>**

**int hmake(struct hvfs **\*_vfs_**);**

# DESCRIPTION

A handle is the user-space equivalent of a file descriptor. Coroutines and channels are represented by handles.

Unlike with file descriptors, however, you can use the **hmake** function to create your own type of handle.

The argument of the function is a virtual-function table of operations associated with the handle.

When implementing the **close** operation, keep in mind that invoking blocking
operations is not allowed, as blocking operations invoked within the context of
a **close** operation will fail with an **ECANCELED** error.

To close a handle, use the **hclose** function.

# RETURN VALUE

Returns the newly allocated handle on success. On error, -1 is returned and _errno_ is set to one of the following values below.

# ERRORS

* **ECANCELED**: Current coroutine is being shut down.
* **EINVAL**: Invalid argument.
* **ENOMEM**: Not enough free memory to create a handle.

