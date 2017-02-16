
Libdill is a C library that makes writing [structured concurrent programs](structured-concurrency.html) easy.

The following example launches two concurrent worker functions that print "Hello!" or "World!", respectively, at random intervals. The program runs for five seconds and then it shuts down.

```c
#include <libdill.h>
#include <stdio.h>
#include <stdlib.h>

coroutine void worker(const char *text) {
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

Code dependent on libdill is compiled like any other C code. The only extra requirement is that it be linked with libdill library:

```
$ cc -ldill -o hello hello.c
```

Libdill is licensed under the MIT/X11 license.

