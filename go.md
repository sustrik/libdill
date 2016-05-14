# go(3) manual page

## NAME

go() - start a coroutine

## SYNOPSIS

```
#include <libdill.h>
int go(expression);
```

## DESCRIPTION

Launches a coroutine that executes the function invocation passed as the argument.

Coroutine is executed in concurrent manner and its lifetime may exceed the lifetime of the caller.

The return value of the function is discarded and cannot be retrieved by the caller.

Any function to be invoked unsing go() must be declared with `coroutine` specifier.

*WARNING*: Coroutine will most likely work even without coroutine specifier. However, it may fail in random non-deterministic fashion, depending on a particular combination of compiler, optimisation level and code in question.

## RETURN VALUE

Returns a coroutine handle. In the case of error it returns -1 and sets `errno` to one of the values below.

## ERRORS

* `ECANCELED`: Current coroutine is in the process of shutting down.
* `ENOMEM`: Not enough memory to allocate the coroutine stack.

## EXAMPLE

```
coroutine void add(int a, int b) {
    printf("%d+%d=%d\n", a, b, a + b);
}

...

int h = go(add(1, 2));
```

## SEE ALSO

* coroutine(3)
* hclose(3)
* proc(3)
