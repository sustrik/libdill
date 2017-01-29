# Libdill build options

You can pass the following options to `./configure`:

* `--disable-shared`: Generate only a static library. This option causes tests to be linked with libdill statically, thereby making debugging easier.
* `--disable-threads`: Can be used with single-threaded programs. It will make libdill a little bit faster and make it not depend on the pthread library.
* `--enable-census`: When this option is set, the library keeps track of stack space used by individual coroutines. It prints statistics when the process exits.
* `--enable-debug`: Add debug info to the library.
* `--enable-valgrind`: Valgrind gets confused by libdill's coroutines. Setting this option helps valgrind make sense of what's going on. It's not 100% foolproof but it helps eliminate many false positives.
