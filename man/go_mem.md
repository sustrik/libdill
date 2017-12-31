# NAME

go_mem - start a coroutine on a user-supplied stack

# SYNOPSIS

**#include &lt;libdill.h>**

**int go_mem(**_expression_**, void** \*_stk_, **size_t** _stklen_**);**

# DESCRIPTION

Launches a coroutine that executes the function invocation passed as argument.  The buffer passed in the _stk_ argument will be used as the coroutine's stack.  The length of the buffer should be specified in the _stklen_ argument.

The coroutine is executed concurrently, and its lifetime may exceed the lifetime of the caller.  The return value of the coroutine, if any, is discarded and cannot be retrieved by the caller.

Any function to be invoked with **go_mem()** must be declared with the **coroutine** specifier.

*WARNING*: Coroutines will most likely work even without the **coroutine** specifier. However, they may fail in random non-deterministic ways, depending on the code in question and the particular combination of a compiler and optimization level. Additionally, arguments to a coroutine must not be function calls. If they are, the program may fail non-deterministically. If you need to pass a result of a computation to a coroutine, do the computation first, and then pass the result as an argument. Instead of:

```c
go_mem(bar(foo(a)), stk, stklen);
```

Do this:

```c
int a = foo();
go_mem(bar(a), stk, stklen);
```

# RETURN VALUE

Returns a coroutine handle. In the case of an error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ENOMEM**: Not enough memory to allocate the coroutine stack.

# EXAMPLE

```c
coroutine void add(int a, int b) {
    printf("%d+%d=%d\n", a, b, a + b);
}

...
char stk[16384];
int h = go_mem(add(1, 2), stk, sizeof(stk));
```

