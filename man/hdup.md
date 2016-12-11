# NAME

hdup - duplicates a handle

# SYNOPSIS

```c
#include <libdill.h>
int hdup(int h);
```

# DESCRIPTION

Duplicates a handle. Both handles will refer to the same underlying object.

Each duplicate of the handle requires it's own call to `hclose`. The underlying object is deallocated once all the handles pointing to it are closed.

# RETURN VALUE

The duplicated handle in case of success or -1 in case of failure. In the latter case `errno` is set to one of the following values.

# ERRORS

* `EBADF`: Invalid handle.

# EXAMPLE

```c
int ch1 = chmake(sizeof(int));
int ch2 = hdup(ch1);
hclose(ch2);
hclose(ch1);
```

