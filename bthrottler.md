# bthrottler(3) manual page

## NAME

bthrottler - bytestream throttler

## SYNOPSIS

```c
#include <dsock.h>
int bthrottler_start(int s,
    uint64_t send_throughput, int64_t send_interval,
    uint64_t recv_throughput, int64_t recv_interval);
int bthrottler_stop(int s);
```

## DESCRIPTION

TODO

## EXAMPLE

TODO

