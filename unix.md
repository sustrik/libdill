# unix(3) manual page

## NAME

unix - UNIX domain protocol

## SYNOPSIS

```c
#include <dsock.h>
int unix_listen(const char *addr, int backlog);
int unix_accept(int s, int64_t deadline);
int unix_connect(const char *addr, int64_t deadline);
int unix_pair(int s[2]);
```

## DESCRIPTION

TODO

## EXAMPLE

TODO

