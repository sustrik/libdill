#
# Copyright (c) 2018 Martin Sustrik  All rights reserved.
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

@VALGRIND_CHECK_RULES@

check_PROGRAMS = \
    example \
    go1 \
    go2 \
    go3 \
    go4 \
    go5 \
    fd \
    handle \
    chan \
    choose \
    sleep \
    signals \
    overload \
    rbtree \
    bundle

if DILL_THREADS
check_PROGRAMS += \
    threads \
    threads2
endif

if DILL_SOCKETS
check_PROGRAMS += \
    ipaddr \
    iol \
    tcp \
    ipc \
    prefix \
    socks5 \
    suffix \
    udp \
    http \
    ws \
    term \
    happyeyeballs \
    tcpmux
endif

if DILL_SOCKETS
if HAVE_TLS
check_PROGRAMS += \
    tls
endif
endif

check_HEADERS = \
    protocol.h \
    utils.h

LDADD = ../libdill.la

TESTS = $(check_PROGRAMS)

