#
# Copyright (c) 2016 Martin Sustrik  All rights reserved.
# Copyright (c) 2013 Luca Barbato
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom
# the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

ACLOCAL_AMFLAGS = -I m4

################################################################################
#  libdill library                                                             #
################################################################################

dillincludedir = $(includedir)
dillinclude_HEADERS = libdill.h libdillimpl.h

lib_LTLIBRARIES = libdill.la

libdill_la_SOURCES = \
    chan.c \
    cr.h \
    cr.c \
    epoll.h.inc \
    epoll.c.inc \
    handle.h \
    handle.c \
    kqueue.h.inc \
    kqueue.c.inc \
    libdill.c \
    list.h \
    now.h \
    now.c \
    poll.h.inc \
    poll.c.inc \
    pollset.h \
    pollset.c \
    qlist.h \
    rbtree.h \
    rbtree.c \
    slist.h \
    stack.h \
    stack.c \
    ctx.h \
    ctx.c \
    utils.h \
    utils.c

if DILL_SOCKETS
libdill_la_SOURCES += \
    bsock.c \
    fd.h \
    fd.c \
    happyeyeballs.c \
    http.c \
    iol.h \
    iol.c \
    ipc.c \
    ipaddr.c \
    msock.c \
    prefix.c \
    socks5.c \
    suffix.c \
    tcp.c \
    term.c \
    udp.c \
    ws.c \
    dns/dns.h \
    dns/dns.c
endif

if DILL_SOCKETS
if HAVE_TLS
libdill_la_SOURCES += \
    tls.c
endif
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libdill.pc

libdill_la_LDFLAGS = \
    -no-undefined -version-info @DILL_LIBTOOL_VERSION@ @PTHREAD_LIBS@

# Turn the source fortification in glibc off - otherwise it would panic
# because of the stack pointer black magic in 'go' macro.
libdill_la_CFLAGS = \
    @PTHREAD_CFLAGS@ \
    -fvisibility=hidden\
    -DDILL_EXPORTS \
    -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

################################################################################
#  automated tests                                                             #
################################################################################

check_PROGRAMS = \
    tests/example \
    tests/go1 \
    tests/go2 \
    tests/go3 \
    tests/go4 \
    tests/go5 \
    tests/fd \
    tests/handle \
    tests/chan \
    tests/choose \
    tests/sleep \
    tests/signals \
    tests/overload \
    tests/rbtree \
    tests/bundle

if DILL_THREADS
check_PROGRAMS += \
    tests/threads \
    tests/threads2
endif

if DILL_SOCKETS
check_PROGRAMS += \
    tests/ipaddr \
    tests/iol \
    tests/tcp \
    tests/ipc \
    tests/prefix \
    tests/socks5 \
    tests/suffix \
    tests/udp \
    tests/http \
    tests/ws \
    tests/term \
    tests/happyeyeballs
endif

if DILL_SOCKETS
if HAVE_TLS
check_PROGRAMS += \
    tests/tls
endif
endif

check_HEADERS = \
    tests/assert.h

LDADD = libdill.la

TESTS = $(check_PROGRAMS)

################################################################################
#  tutorials                                                                   #
################################################################################

noinst_PROGRAMS =

if DILL_SOCKETS
noinst_PROGRAMS += \
    tutorial/basics/step1 \
    tutorial/basics/step2 \
    tutorial/basics/step3 \
    tutorial/basics/step4 \
    tutorial/basics/step5 \
    tutorial/basics/step6 \
    tutorial/protocol/step1 \
    tutorial/protocol/step2 \
    tutorial/protocol/step3 \
    tutorial/protocol/step4 \
    tutorial/protocol/step5 \
    tutorial/protocol/step6 \
    tutorial/protocol/step7 \
    tutorial/protocol/step8 \
    tutorial/protocol/step9
if HAVE_TLS
noinst_PROGRAMS += \
    tutorial/sockets/step1 \
    tutorial/sockets/step2 \
    tutorial/sockets/step3 \
    tutorial/sockets/step4 \
    tutorial/sockets/step5
endif
endif

################################################################################
#  examples                                                                    #
################################################################################

if DILL_SOCKETS
noinst_PROGRAMS += \
    examples/socks5proxy \
    examples/webapp
endif

################################################################################
#  performance tests                                                           #
################################################################################

noinst_PROGRAMS += \
    perf/go \
    perf/ctxswitch \
    perf/chan \
    perf/choose \
    perf/done \
    perf/whispers \
    perf/timer

################################################################################
#  manpage documentation generation                                            #
################################################################################

SUBDIRS=man

################################################################################
#  additional packaging-related stuff                                          #
################################################################################

#  Generate ChangeLog file from git.
#  Also, there's no git availabe when building from the source package and
#  thus no way to obtain package version. Therefore, package version is
#  saved into a file when building a source package.
dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog; \
		cd "$(top_srcdir)"; \
		(echo '# Generated by Makefile. Do not edit.'; echo; \
		  ./missing --run git log --decorate ) > ChangeLog.tmp; \
		mv -f ChangeLog.tmp $(top_distdir)/ChangeLog; \
		rm -f ChangeLog.tmp; \
    else \
        cp -f ChangeLog $(top_distdir)/ChangeLog || \
        echo Failed to generate ChangeLog >&2; \
	fi; \
	$(srcdir)/package_version.sh > $(distdir)/.version

EXTRA_DIST = \
    ./abi_version.sh \
    ./package_version.sh

CLEANFILES = *.gcno *.gcda dns/*.gcno dns/*.gcda tests/*.gcno tests/*.gcda

distclean-local:
	-rm -f config.h
