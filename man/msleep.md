# NAME

msleep - waits until deadline

# SYNOPSIS

```c
#include <libdill.h>
int msleep(int64_t deadline);
```

# DESCRIPTION

Waits until the deadline expires.

# RETURN VALUE

Returns 0 in case of success, -1 in case of error. In the latter case it sets `errno` to one of the values below.

# ERRORS

* `ECANCELED`: Current coroutine is being shut down.

# EXAMPLE

```c
msleep(now() + 1000);
```

