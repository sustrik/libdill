
# NAME

chmake_mem - create a channel in user-supplied memory

# SYNOPSIS

**#include &lt;libdill.h>**

**int chmake_mem(void **\*_mem_**, int **_chv_**[2]);**

# DESCRIPTION

Creates a bidirectional channel in user-supplied memory.

_mem_ is the memory of at least _CHSIZE_ bytes to store channel data in. The memory must not be deallocated before all handles referring to the channel are closed with the **hclose** function, or the behavior is undefined.

Do not use this function unless you are hyper-optimizing your code and you want to avoid a single memory allocation per channel. Whenever possible, use **chmake** instead.

The channel is a synchronization primitive, not a container. It doesn't store any items.

# RETURN VALUE

Returns a channel handle. In the case of an error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.
* **EINVAL**: Invalid parameter.

# EXAMPLE

```c
struct chitem {
    int h1;
    int h2;
    char mem[CHSIZE];
};

struct chitem *alloc_channels(size_t n) {
    struct chitem *a = malloc(sizeof(struct chitem) * n);
    int i;
    for(i = 0; i != n; ++i) {
        int ch[2];
        int rc = chmake_mem(a[i].mem, ch);
        a[i].h1 = ch[0];
        a[i].h2 = ch[1];
    }
    return a;
}

void free_channels(struct chitem *a, size_t n) {
    int i;
    for(i = 0; i != n; ++i) {
        hclose(a[i].h1);
        hclose(a[i].h2);
    }
    free(a);
}
```

