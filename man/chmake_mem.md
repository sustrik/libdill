# NAME

chmake_mem - create a channel in user-supplied memory

# SYNOPSIS

```c
#include <libdill.h>
struct chmem;
int chmake_mem(size_t itemsz, struct chmem *mem);
```

# DESCRIPTION

Creates a channel in user-supplied memory.

First parameter is the size of the items to be sent through the channel, in bytes.

Second parameter is the memory to store channel data in. The memory cannot be deallocated before all handles referring to the channel are closed using `hclose` function. Otherwise, undefined behaviour ensues.

Do not use this function unless you are hyper-otimizing your code and you want to avoid a single memory allocation per channel. Whenever possible, use `chmake` instead.

The channel is a synchronizationn primitive, not a container. It doesn't store any items.

# RETURN VALUE

Returns a channel handle. In the case of error it returns -1 and sets `errno` to one of the values below.

# ERRORS

* `ECANCELED`: Current coroutine is in the process of shutting down.
* `EINVAL`: Invalid parameter.

# EXAMPLE

```c
struct chitem {
    int h;
    struct chmem mem;
};

struct chitem *alloc_channels(size_t n) {
    struct chitem *a = malloc(sizeof(struct chitem) * n);
    int i;
    for(i = 0; i != n; ++i)
        a[i].h = chmake_mem(0, &a[i].mem);
    return a;
}

void free_channels(struct chitem *a, size_t n) {
    int i;
    for(i = 0; i != n; ++i)
        hclose(a[i].h);
    free(a);
}
```

