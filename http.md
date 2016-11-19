# http(3) manual page

## NAME

http - http protocol

## SYNOPSIS

```c
#include <dsock.h>
int http_start(int s);
int http_done(int s, int64_t deadline);
int http_stop(int s, int64_t deadline);
int http_sendrequest(int s, const char *command, const char *resource,
    int64_t deadline);
int http_recvrequest(int s, char *command, size_t commandlen, char *resource,
    size_t resourcelen, int64_t deadline);
int http_sendstatus(int s, int status, const char *reason, int64_t deadline);
int http_recvstatus(int s, char *reason, size_t reasonlen, int64_t deadline);
int http_sendfield(int s, const char *name, const char *value, int64_t deadline);
int http_recvfield(int s, char *name, size_t namelen, char *value,
    size_t valuelen, int64_t deadline);
```

## DESCRIPTION

TODO

## EXAMPLE

TODO

