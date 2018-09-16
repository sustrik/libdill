# Libdill build options

When including a header file (`libdill.h` or `libdillimpl.h`) you can define following macros:

* `DILL_DISABLE_SOCKETS`: Don't declare socket-related symbols.
* `DILL_DISABLE_RAW_NAMES`: Each name in libdill has two variants. One is raw ("go", "WS_TEXT", "struct iolist"), another one has a prefix ("dill_go", "DILL_WS_TEXT", "struct dill_iolist"). Defining this macro causes raw variants not to be declared. Use this option if you want to avoid name colisions with third party header files.

When building the library you can pass the following options to `./configure`:

* `--disable-shared`: Generate only a static library. This option causes tests to be linked with libdill statically, thereby making debugging easier.
* `--disable-sockets`: Don't build libdill's socket library. Build only the core functionality.
* `--disable-threads`: Can be used with single-threaded programs. It will make libdill a little bit faster and make it not depend on the pthread library.
* `--enable-census`: When this option is set, the library keeps track of stack space used by individual coroutines. It prints statistics when the process exits.
* `--enable-debug`: Add debug info to the library.
* `--enable-gcov`: Generate coverage report using gcov.
* `--enable-tls`: Build TLS protocol. To be able to build with this option you need OpenSSL 1.1.0. or later installed on your machine.
* `--enable-valgrind`: Valgrind gets confused by libdill's coroutines. Setting this option helps valgrind make sense of what's going on. It's not 100% foolproof but it helps eliminate many false positives.
