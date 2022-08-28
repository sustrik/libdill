# Testing libdill with QuickCheck
Demonstrates how to test monadic C-code using [QuickCheck](https://en.wikipedia.org/wiki/QuickCheck) and property-based testing.

## Environment
In order to run these tests at least the following is needed:
- A C compiler
- [Stack](https://haskellstack.org/)

## Building
Build the library from the top-level of the libdill repo:

```shell
./autogen.sh
./configure --enable-debug
make
```

## Testing
Test the library with QuickCheck:

```shell
cd test/quickcheck
stack test
```

Test the library in GHCi:
```shell
cd test/quickchek
stack ghci libdill-quickcheck:libdill-quickcheck-test
```

and then in the GHCi prompt
```shell
> main
+++ OK, passed 10000 tests.
```
