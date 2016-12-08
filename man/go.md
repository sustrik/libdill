# NAME

go - start a coroutine

# SYNOPSIS

```c
#include <libdill.h>
int go(expression);
```

# DESCRIPTION

Launches a coroutine that executes the function invocation passed as the argument.

Coroutine is executed in concurrent manner and its lifetime may exceed the lifetime of the caller.

The return value of the coroutine, if any, is discarded and cannot be retrieved by the caller.

Any function to be invoked using go() must be declared with `coroutine` specifier.

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
* `ENOMEM`: Not enough memory to allocate the coroutine stack.

# EXAMPLE

```c
coroutine void add(int a, int b) {
    printf("%d+%d=%d\n", a, b, a + b);
}

...

int h = go(add(1, 2));
```

