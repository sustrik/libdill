# NAME

hmake - creates a handle

# SYNOPSIS

```c
#include <libdill.h>

struct hvfs {
    void *(*query)(struct hvfs *vfs, const void *type);
    void (*close)(int h);
};

int hmake(struct hvfs *vfs);
```

# DESCRIPTION

Handle is a user-space equivalent of a file descriptor. Coroutines and channels are represented by handles.

Unlike with file descriptors though, you can create your own handle type. To do so use `hmake` function.

The argument is a virtual function table of operations associated with the handle.

When implementing the close operation keep in mind that it is not allowed to invoke blocking operations. Blocking operations invoked in the context of close operation will fail with `ECANCELED` error.

To close a handle use `hclose` function.

# RETURN VALUE

In case of success the function returns a newly allocated handle. In case of failure it returns -1 and sets `errno` to one of the values below.

# ERRORS

* `ECANCELED`: Current coroutine is being shut down.
* `EINVAL`: Invalid argument.
* `ENOMEM`: Not enough free memory to create the handle.

