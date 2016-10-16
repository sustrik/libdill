# proc(3) manual page

## NAME

proc - start a process

## SYNOPSIS

```c
#include <libdill.h>
int proc(expression);
```

## DESCRIPTION

Launches a process that executes the function invocation passed as the argument.

The return value of the invoked function, if any, is discarded and cannot be retrieved by the caller.

WARNING: `proc` is a thin wrapper on top of `fork`, therefore it shares the drawbacks of `fork`. Specifically, all the resources allocated in the parent process are duplicated to the child process. Unless the child deallocates them explicitly, they become, effectively, resource leaks. Therefore, the best way to use `proc` is to spawn all the child processes as soon as the parent process is started and refrain from creating more child processes later on.

## RETURN VALUE

Returns a process handle. In the case of error it returns -1 and sets `errno` to one of the values below.

## ERRORS

* `ENOMEM`: New process cannot be created either because there's not enough free memory available or one the the following reasons:
  1. `CHILD_MAX` would be exceeded.
  2. `OPEN_MAX` would be exceeded.
  3. The number of simultaneously open files in the system would exceed a system-imposed limit.
* `ECANCELED`: Current coroutine is in the process of shutting down.

## EXAMPLE

```c
coroutine void add(int a, int b) {
    printf("%d+%d=%d\n", a, b, a + b);
}

...

int h = proc(add(1, 2));
```

