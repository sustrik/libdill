
# Past versions

**Mar 11th, 2017 <http://libdill.org/libdill-1.4.tar.gz>**

* TCP and IPC (AF_UNIX) support added.
* libdillimpl.h created for stuff needed by protocol implementors.
* hmake() and hquery() functions moved to libdillimpl.h

**Feb 25th, 2017 <http://libdill.org/libdill-1.3.tar.gz>**

* DNS resolution functions added.
* hdone() added.
* Bug fixes.
* Performance improvements.

**Feb 5th, 2017 <http://libdill.org/libdill-1.2.tar.gz>**

* If the CPU is at 100%, external events, such as those from file descriptors, are checked once per second.
* Multiple optimizations.
* Multiple bug fixes.

**Jan 1st, 2017 <http://libdill.org/libdill-1.1.tar.gz>**

* Multiple bugfixes.
* Multiple performance enhancements.
* fdclean() returns EBUSY error if file descriptor is being waited for by a different coroutine.

**Dec 16th, 2016 <http://libdill.org/libdill-1.0.tar.gz>**

* Final API cleanup before the first official release
    - `channel` renamed to `chmake`
    - `go_stack` renamed to `go_mem`

* Simplification of channel semantics
    - Channels are always unbuffered
    - `choose` has deterministic behavior

* `chmake_mem` function added

* Man pages added to the package

* Some bug fixes

**Dec 6th, 2016 <http://libdill.org/libdill-0.11-beta.tar.gz>**

* Make the library [thread-friendly](threads.html).

**Nov 27, 2016 <http://libdill.org/libdill-0.10-beta.tar.gz>**

* Rename hcreate() to hmake(). hcreate() clashes with the POSIX function of the same name.

**Nov 19, 2016 <http://libdill.org/libdill-0.9-beta.tar.gz>**

* A lot of optimizations. A context switch has been seen to execute in 6ns, coroutine creation in 26ns and passing a message through a channel in 40ns.

* The new `go_stack()` construct allows running coroutines on a user-allocated stack.

* The New `--enable-census` option monitors coroutines' stack usage and prints statistics on process shutdown.

* Mutliple bug fixes.

**Oct 16, 2016 <http://libdill.org/libdill-0.8-beta.tar.gz>**
**Oct 13, 2016 <http://libdill.org/libdill-0.7-beta.tar.gz>**
**Sep 07, 2016 <http://libdill.org/libdill-0.6-beta.tar.gz>**
**Jun 03, 2016 <http://libdill.org/libdill-0.5-beta.tar.gz>**
**May 22, 2016 <http://libdill.org/libdill-0.4-beta.tar.gz>**
**May 15, 2016 <http://libdill.org/libdill-0.3-beta.tar.gz>**

