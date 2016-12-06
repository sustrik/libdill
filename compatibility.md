
# Compatibility

## Operating Systems

Currently only modern POSIX systems are supported; it has been tested mainly on Linux, FreeBSD and OSX. Please send information if it fails on a particular BSD system. It is likely to work with DragonFlyBSD. NetBSD and OpenBSD probably need `DILL_THREAD_FALLBACK` to work as Thread Local Storage is unsupported.

### Cygwin

Cygwin is very broken, it does not support AF_UNIX properly so no development will progress further with this platform. Work will be done porting libdill to use Mingw and Windows IOCP instead, help is welcome.

## Compiler

libdill requires either GCC and Clang. Your mileage may vary with other compilers:

- Intel's compiler may work, please send a message along if you find it does.
- MSVC compiler will not work, for both reasons of POSIX and C-extensions used.

The non-standard language features that are required by libdill are as follows:

- The GCC-extension [statement expressions(https://gcc.gnu.org/onlinedocs/gcc-3.2/gcc/Statement-Exprs.html) is used in the `go` macro.
- GCC-style inline assembly.

Compiler features and incompatibilities:

- On x86 and x86-64 platforms, `DILL_ARCH_FALLBACK` option is incompatible with GCC 6.2.1 when `-fstack-protector` is enabled. Please compile libdill and your application with `-fno-stack-protector` if you need to use the fallback. Non-x86 platforms have not been tested with GCC 6.2.1, please report your mileage.
- Address sanitisation and other stack checking features of compilers are likely to break a `libdill` build.
- x32 addressing probably does not work, this has not been tested yet.

## Standard Libraries

### glibc

Incompatibilities:

- Stack fortification is incompatible with libdill. Do not override the default `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0` in a libdill build. However, your application may use stack fortification.
- Stack protection does not work with `DILL_ARCH_FALLBACK` option. To make it work try using `-fno-stack-protector` option.

### musl

Compatible.
