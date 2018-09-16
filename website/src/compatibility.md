
# Compatibility

### Operating Systems

Currently, only modern POSIX systems are supported. The library has been tested mainly on Linux, FreeBSD, and OSX.

It is likely to work with DragonFlyBSD as well. NetBSD and OpenBSD will probably need `DILL_THREAD_FALLBACK` to work as they don't have support for thread local storage. If you are using libdill on these platforms, please let us know.

There is currently no support for Windows. Cygwin is very broken. It doesn't support `AF_UNIX` properly, and so no further development will be done for this platform. 
Libdill is planned to be ported to Mingw and Windows IOCP instead. Help is welcome.

### Compilers

libdill requires either GCC and Clang. Your mileage with other compilers may vary:

- Intel compilers may work; please send us a message if you find they do
- The MSVC compiler won't work due to the POSIX features and C extensions in use

The non-standard language features libdill requires are as follows:

- The [statement expressions](https://gcc.gnu.org/onlinedocs/gcc-3.2/gcc/Statement-Exprs.html) GCC-extension, which is used in the `go` macro.
- GCC-style inline assembly.

Compiler features and incompatibilities:

- On x86 and x86-64 platforms, the `DILL_ARCH_FALLBACK` option is incompatible with GCC 6.2.1 when `-fstack-protector` is enabled. Please compile libdill and your application with `-fno-stack-protector` if you need to use the fallback. Non-x86 platforms have not been tested with GCC 6.2.1. Please report your mileage.
- Address sanitization and other stack checking compiler features are likely to break your `libdill` build.
- x32 addressing probably doesn't work, this has not been tested yet.

### Standard Libraries

#### glibc

Incompatibilities:

- Stack fortification is incompatible with libdill. Do not override the default `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0` in a libdill build. Using fortification in your libdill application is possible.
- Stack protection doesn't work with the `DILL_ARCH_FALLBACK` option. To make it work, try using the `-fno-stack-protector` option.

#### musl

Compatible.
