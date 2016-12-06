# Libdill build options

You can pass the following options to `./configure`:

* `--disable-shared`: Generate only static library. This option causes tests to be statically linked with libdill thus making debugging easier.
* `--disable-threads`: Can be used with single-threaded programs. It will make libdill a little bit faster and make it not depend on pthread library.
* `--enable-census`: When this option is set the library keeps track of space on stack was used by individual coroutines. It prints the statistics when the process exits.
* `--enable-debug`: Add debug info to the library.
* `--enable-valgrind`: Valgrind gets confused by libdill's coroutines. Setting this option helps valgrind make sense of what's going on. It's not 100% waterproof but it helps to eliminate many false positives.
