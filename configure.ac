#
# Copyright (c) 2013 Luca Barbato
# Copyright (c) 2015 Martin Sustrik  All rights reserved.
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

################################################################################
#  Start the configuration phase.                                              #
################################################################################

AC_PREREQ([2.53])

AC_INIT([libdill], [m4_esyscmd([./package_version.sh])],
   [sustrik@250bpm.com], [libdill], [http://libdill.org/])
AC_CONFIG_SRCDIR([libdill.pc.in])
AM_INIT_AUTOMAKE([1.6 foreign subdir-objects tar-ustar])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CANONICAL_HOST

################################################################################
#  Retrieve the versions.                                                      #
################################################################################

AC_PROG_SED
AC_PROG_AWK

DILL_ABI_VERSION=m4_esyscmd([./abi_version.sh])
DILL_PACKAGE_VERSION=m4_esyscmd([./package_version.sh])
DILL_LIBTOOL_VERSION=m4_esyscmd([./abi_version.sh -libtool])

AC_SUBST(DILL_ABI_VERSION)
AC_SUBST(DILL_PACKAGE_VERSION)
AC_SUBST(DILL_LIBTOOL_VERSION)

AC_MSG_NOTICE([libdill package version: $DILL_PACKAGE_VERSION])
AC_MSG_NOTICE([libdill ABI version: $DILL_ABI_VERSION])

################################################################################
#  Check the compilers.                                                        #
################################################################################

AC_PROG_CC_C99
AM_PROG_CC_C_O

################################################################################
#  --enable-debug                                                              #
################################################################################

AC_ARG_ENABLE([debug], [AS_HELP_STRING([--enable-debug],
    [Enable debugging information [default=no]])])

if test "x$enable_debug" = "xyes"; then
    # Override original optimisation level - last option specified wins.
    CFLAGS="$CFLAGS -g -O0"
fi

################################################################################
#  --enable-valgrind                                                           #
################################################################################

AC_ARG_ENABLE([valgrind], [AS_HELP_STRING([--enable-valgrind],
    [Provide information about coroutine stacks to valgrind [default=no]])])

if test "x$enable_valgrind" = "xyes"; then
    AC_DEFINE(DILL_VALGRIND)
fi

################################################################################
#  --enable-gcov                                                               #
################################################################################

AC_ARG_ENABLE([gcov], [AS_HELP_STRING([--enable-gcov],
    [Enable gcov [default=no]])])

if test "x$enable_gcov" = "xyes"; then
    CFLAGS="$CFLAGS --coverage --no-inline -O0"
fi

################################################################################
#  --enable-census                                                             #
################################################################################

AC_ARG_ENABLE([census], [AS_HELP_STRING([--enable-census],
    [Print out information about space used on coroutine stacks [default=no]])])

if test "x$enable_census" = "xyes"; then
    AC_DEFINE(DILL_CENSUS)
fi

################################################################################
#  --disable-threads                                                           #
################################################################################

AC_ARG_ENABLE([threads], [AS_HELP_STRING([--disable-threads],
    [Disable threading support [default=no]])])

if test "x$enable_threads" = "xno"; then
    AM_CONDITIONAL([DILL_THREADS], false)
else
    PTHREAD_LIBS=error
    PTHREAD_CFLAGS=""
    AC_CHECK_LIB([pthread], [pthread_attr_init], PTHREAD_LIBS="-lpthread")
    if test "x$PTHREAD_LIBS" = "xerror"; then
            AC_CHECK_LIB([c_r], [pthread_attr_init], PTHREAD_LIBS="-lc_r")
    fi
    if test "x$PTHREAD_LIBS" = "xerror"; then
            AC_CHECK_FUNC([pthread_attr_init], PTHREAD_LIBS="")
    fi
    if test "x$PTHREAD_LIBS" = "xerror"; then
        AC_MSG_ERROR([no thread implementation available])
    else
        PTHREAD_CFLAGS="-pthread"
        AC_DEFINE(DILL_THREADS)
        AC_DEFINE(DILL_PTHREAD)
        AM_CONDITIONAL([DILL_THREADS], true)
    fi
    AC_SUBST(PTHREAD_LIBS)
    AC_SUBST(PTHREAD_CFLAGS)
fi

################################################################################
#  --disable-sockets                                                           #
################################################################################

AC_ARG_ENABLE([sockets], [AS_HELP_STRING([--disable-sockets],
    [Disable socket support [default=no]])])

if test "x$enable_sockets" = "xno"; then
    AM_CONDITIONAL([DILL_SOCKETS], false)
else
    AM_CONDITIONAL([DILL_SOCKETS], true)
fi

################################################################################
#  --enable-tls
################################################################################

AC_ARG_ENABLE([tls], [AS_HELP_STRING([--enable-tls],
    [Enable TLS sockets [default=no]])])

AM_CONDITIONAL([HAVE_TLS], [test "x$enable_tls" = xyes])

if test "x$enable_tls" = "xyes"; then
    AC_DEFINE(HAVE_TLS)
    AC_CHECK_LIB([ssl], [SSL_ctrl ], [],
        AC_MSG_ERROR([libssl not found; install OpenSSL]))
    AC_CHECK_LIB([crypto], [BIO_read], [],
        AC_MSG_ERROR([libcrypto not found; install OpenSSL]))
fi

################################################################################
#  Feature checks.                                                             #
################################################################################

AC_CHECK_FUNC([posix_memalign], [AC_DEFINE([HAVE_POSIX_MEMALIGN])])
AC_CHECK_FUNC([mprotect], [AC_DEFINE([HAVE_MPROTECT])])
AC_CHECK_LIB([rt], [clock_gettime])
AC_CHECK_FUNCS([clock_gettime])
AC_CHECK_LIB([socket], [socket])
AC_CHECK_FUNCS([epoll_create], [AC_DEFINE([HAVE_EPOLL])])
AC_CHECK_FUNCS([kqueue], [AC_DEFINE([HAVE_KQUEUE])])

dnl Check if struct sockaddr contains sa_len member
AC_CHECK_MEMBERS([struct sockaddr.sa_len], [], [], [
# include <sys/types.h>
# include <sys/socket.h>
])

################################################################################
#  Libtool                                                                     #
################################################################################

LT_INIT

################################################################################
#  --enable-shared                                                             #
################################################################################

if test "x$enable_shared" = "xyes"; then
    AC_DEFINE(DILL_SHARED)
fi

################################################################################
#  Finish the configuration phase.                                             #
################################################################################

AC_CONFIG_MACRO_DIR([m4])

AC_OUTPUT([Makefile man/Makefile libdill.pc])
cp confdefs.h config.h

