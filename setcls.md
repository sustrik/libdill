# setcls(3) manual page

## NAME

setcls() - set coroutine-local storage

## SYNOPSIS

```
#include <libdill.h>
void setcls(void *val);
```

## DESCRIPTION

Set coroutine-local storage, a single pointer that will be accessible from anywhere within the same coroutine using `cls` function.

## RETURN VALUE

None.

## ERRORS

None.

## EXAMPLE

```
char *str = "Hello, world!";
setcls(str);
printf("%s\n", (char*)cls());
```

## SEE ALSO

[cls(3)](cls.html)
[go(3)](go.html)

## AUTHORS

Martin Sustrik <sustrik@250bpm.com>

