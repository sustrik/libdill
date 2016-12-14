# NAME

go_mem - start a coroutine on a user-supplied stack

# SYNOPSIS

```c
#include <libdill.h>
int go_mem(expression, void *stk, size_t stklen);
```

# DESCRIPTION

Launches a coroutine that executes the function invocation passed as the argument. The buffer passed in `stk` argument will be used as coroutine stack. The length of the buffer should be specified in `stklen` argument.

Coroutine is executed in concurrent manner and its lifetime may exceed the lifetime of the caller.

The return value of the coroutine, if any, is discarded and cannot be retrieved by the caller.

Any function to be invoked using go_mem() must be declared with `coroutine` specifier.

*WARNING*: Coroutine will most likely work even without `coroutine` specifier. However, it may fail in random non-deterministic fashion, depending on a particular combination of compiler, optimisation level and code in question. Also, all arguments to a coroutine must not be function calls. If they are the program may fail non-deterministically. If you need to pass a result of a computation to a coroutine do the computation first, then pass the result as an argument. Instead of:

```c
go(bar(foo(a)));
```

Do this:

```c
int a = foo();
go(bar(a));
```

# RETURN VALUE

Returns a coroutine handle. In the case of error it returns -1 and sets `errno` to one of the values below.

# ERRORS

* `ECANCELED`: Current coroutine is in the process of shutting down.
* `ENOMEM`: The stack buffer is not large enough to hold the common coroutine bookkeeping info.

# EXAMPLE

```c
coroutine void add(int a, int b) {
    printf("%d+%d=%d\n", a, b, a + b);
}

...
char stk[16384];
int h = go_mem(add(1, 2), stk, sizeof(stk));
```

