
# Past versions

**Jan 1st, 2016 <http://libdill.org/libdill-1.1.tar.gz>**

* Multiple bugfixes.
* Multiple performance enhancements.
* fdclean() returns EBUSY error if file descriptor is being waited for by a different coroutine.

**Dec 16th, 2016 <http://libdill.org/libdill-1.0.tar.gz>**

* Final API cleanup before the first official release
    - `channel` renamed to `chmake`
    - `go_stack` renamed to `go_mem`

* Simplification of channel semantics
    - Channels are always unbuffered
    - `choose` has deterministic behaviour

* `chmake_mem` function added

* Man pages added to the package

* Some bug fixes

**Dec 6th, 2016 <http://libdill.org/libdill-0.11-beta.tar.gz>**

* Make the library [thread-friendly](threads.html).

**Nov 27, 2016 <http://libdill.org/libdill-0.10-beta.tar.gz>**

* Rename hcreate() to hmake(). hcreate() clashes with POSIX function of the same name.

**Nov 19, 2016 <http://libdill.org/libdill-0.9-beta.tar.gz>**

* A lot of optimizations. Context switch have been seen to execute in 6ns, coroutine creation in 26ns. Passing a message via a channel in 40ns.

* New `go_stack()` construct allows to run coroutine on a user-allocated stack.

* New `--enable-census` option monitors coroutines' stack usage and prints thr statistics on process shutdown.

* Mutliple bug fixes.

**Oct 16, 2016 <http://libdill.org/libdill-0.8-beta.tar.gz>**
**Oct 13, 2016 <http://libdill.org/libdill-0.7-beta.tar.gz>**
**Sep 07, 2016 <http://libdill.org/libdill-0.6-beta.tar.gz>**
**Jun 03, 2016 <http://libdill.org/libdill-0.5-beta.tar.gz>**
**May 22, 2016 <http://libdill.org/libdill-0.4-beta.tar.gz>**
**May 15, 2016 <http://libdill.org/libdill-0.3-beta.tar.gz>**

