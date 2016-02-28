<link rel="stylesheet" type="text/css" href="main.css">

# libdill: Structured Concurrency for C

Libdill is a C library that makes writing concurrent programs easy.

The following example launches two concurrent worker functions that print
"Hello!" or "World!", respectively, in random intervals. Program runs for
five seconds, then it shuts down.

```
#include <libdill.h>
#include <stdio.h>
#include <stdlib.h>

coroutine int worker(const char *text) {
    while(1) {
        printf("%s\n", text);
        msleep(now() + random() % 500);
    }
}

int main() {
    go(worker("Hello!"));
    go(worker("World!"));
    msleep(now() + 5000);
    return 0;
}
```

## Installation

```
$ git clone git@github.com:sustrik/libdill.git
$ cd libdill
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
```

## Compilation

Code using libdill is compiled in standard C way. The only additional
requirement is to link it with libdill library:

```
gcc -ldill -o hello hello.c
```
