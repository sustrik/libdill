
# Past versions

**Oct 29th, 2018 <http://libdill.org/libdill-2.13.tar.gz>**

* An implementation of Happy Eyeballs protocol (RFC 8305) added.
* Function ipaddr_remotes() added.
* A bug in resolving IPv6 addresses fixed.

**Oct 18th, 2018 <http://libdill.org/libdill-2.12.tar.gz>**

* A bug in TLS protocol fixed.
* A memory leak in HTTP protocol fixed.

**Sept 23th, 2018 <http://libdill.org/libdill-2.11.tar.gz>**

* Couple of OSX bugs fixed

**Sept 16th, 2018 <http://libdill.org/libdill-2.10.tar.gz>**

* C++ guards added to libdill.h
* Tutorials fixed

**Jul 20th, 2018 <http://libdill.org/libdill-2.9.tar.gz>**

* "fromfd" functions added allowing to wrap native OS sockets into lidill's socket classes.
* Couple of bug fixes.

**Apr 27th, 2018 <http://libdill.org/libdill-2.8.tar.gz>**

* hdone() function was removed and replaced by type-specific done functions (e.g. "chdone", "tcp_done").
* hdup() was replaced by hown() with different semantics (transfer of ownership).
* PFX protocol renamed to PREFIX.
* CRLF protocol renamed to SUFFIX, allows to sepcify arbitrary delimiter, not just CRLF.
* Terminal handshake removed from PREFIX and SUFFIX protocols.
* New protocol TERM added. It does terminal handshakes.
* EBUSY error if a socket is used from multiple coroutines in parallel.
* All the symbols exported from the library have "dill" prefix.
* Macros to expand raw names to dill-prefixed names (chsend -> dill_chsend).
* DILL_DISABLE_RAW_NAMES disables the raw name macros, prevents name clashes with different libraries.
* Funcion ipaddr_equal() added. Compares two IP addresses.
* Handle numbers are reused much more rarely.
* Many bugfixes.

**Apr 1st, 2018 <http://libdill.org/libdill-2.7.tar.gz>**

* All mem functions take a structure instead of a raw byte buffer
    * More compile-time safety checks
    * Alignment done by compiler rather than left to the user
* PFX protocol is parametrized
    * Customizable size of the 'size' field
    * Customizable endianness of the field
* Several bug fixes.

**Mar 23rd, 2018 <http://libdill.org/libdill-2.6.tar.gz>**

* Experimental support for WebSockets added

**Mar 3rd, 2018 <http://libdill.org/libdill-2.5.tar.gz>**

* Bug in TLS protocol solved
* Function budle_wait added
* Brand new man pages

**Feb 17th, 2018 <http://libdill.org/libdill-2.4.tar.gz>**

* Experimental TLS protocol
* Experimental HTTP protocol

**Feb 10th, 2018 <http://libdill.org/libdill-2.3.tar.gz>**

* tcp_listen_mem
* tcp_accept_mem
* tcp_connect_mem
* ipc_listen_mem
* ipc_accept_mem
* ipc_connect_mem
* ipc_pair_mem
* crlf_attach_mem
* pfx_attach_mem
* udp_open_mem

**Feb 4th, 2018 <http://libdill.org/libdill-2.2.tar.gz>**

* hdone() can be called on a bundle to wait for coroutines to finish

**Feb 1st, 2018 <http://libdill.org/libdill-2.1.tar.gz>**

* Coroutine bundles added
* struct chmem replaced by CHSIZE macro
* hctrl removed

**Jan 15th, 2018 <http://libdill.org/libdill-2.0.tar.gz>**

* Channels are untyped
* Channels are bidirectional
* A control channel is associated with each coroutine

**Dec 28th, 2017 <http://libdill.org/libdill-1.7.tar.gz>**

* UDP sockets added
* Minor bug fixes

**Apr 8th, 2017 <http://libdill.org/libdill-1.6.tar.gz>**

* Bug fix release.

**Mar 17th, 2017 <http://libdill.org/libdill-1.5.tar.gz>**

* Two simple framing protocols added (CRLF and PFX).
* hdone() function has deadline parameter.
* Support for gcov added.

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

