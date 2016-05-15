
## Reference

* [channel](channel.html)
* [chdone](chdone.html)
* [choose](choose.html)
* [chrecv](chrecv.html)
* [chsend](chsend.html)
* [cls](cls.html)
* [fdclean](fdclean.html)
* [fdin](fdin.html)
* [fdout](fdout.html)
* [go](go.html)
* [handle](handle.html)
* [hclose](hclose.html)
* [hdata](hdata.html)
* [hdup](hdup.html)
* [msleep](msleep.html)
* [now](now.html)
* [proc](proc.html)
* [setcls](setcls.html)
* [yield](yield.html)


## Reporting bugs

Report a bug here:

<https://github.com/sustrik/libdill/issues>

## Accessing source code

To clone the repository:

```
$ git clone https://github.com/sustrik/libdill.git
```

## Building from source

```
$ ./autogen.sh
$ ./configure
$ make
$ make check
$ sudo make install
```

## Debugging

For easy debugging use the following configure options:

```
$ ./configure --disable-shared --enable-debug --enable-valgrind
```

The above will turn the optimisation out, generate debug symbols and link all the tests with the static version of the library. The second option means that the executables in `tests` subdirectory will be actual debuggable binaries rather that wrapper shell scripts. The last option instructs valgrind about where are the coroutine stacks located and thus prevents spurios valgrind warnings.

## Continuous integration

<iframe width="100%" height="20" frameBorder="0"
src="https://api.travis-ci.org/sustrik/libdill.svg?branch=master"></iframe>

Travis: <https://travis-ci.org/sustrik/libdill>

## Contributing

To contribute to libdill send your patch to the mailing list or, alternatively, send a GitHub pull request. In either case you have to state that your patch is submitted under MIT/X11 license, so that it can be incorporated into the mainline codebase without licesing issues.

If you make a substantial contribution to a file, add your copyright to the file header. Irrespective of whether you do so, your name will be added to the AUTHORS file to indicate you own copyright to a part of the codebase.

## Release process

These instructions are intended for the project maintainers:

* Run `make distcheck` to check whether the packaging process still works.
* Bump ABI version as appropriate (see here: <http://250bpm.com/blog:41>).
* Commit it and push it back to the master on GitHub.
* Tag the new version and push the tag to GitHub (e.g. `git tag -a 0.3-beta; git push origin 0.3-beta`).
* Clone a clean repo from GitHub.
* Build the package (`./autogen.sh; ./configure; make distcheck`).
* Get the checksum (`sha1sum`) of the package.
* Add the package to `gh-pages` branch.
* Adjust the download.html web page in `gh-pages` branch.
* Commit and push to `gh-pages`.
* Send the announcement about the release to the project mailing list.

