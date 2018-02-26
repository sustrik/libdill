#
# Copyright (c) 2017 Martin Sustrik  All rights reserved.
# Copyright (c) 2016 Tai Chi Minh Ralph Eastwood
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

man3_MANS = \
    bundle.3 \
    bundle.html \
    bundle_go.3 \
    bundle_go.html \
    bundle_go_mem.3 \
    bundle_go_mem.html \
    bundle_mem.3 \
    bundle_mem.html \
    chmake.3 \
    chmake.html \
    chmake_mem.3 \
    chmake_mem.html \
    choose.3 \
    choose.html \
    chrecv.3 \
    chrecv.html \
    chsend.3 \
    chsend.html \
    fdclean.3 \
    fdclean.html \
    fdin.3 \
    fdin.html \
    fdout.3 \
    fdout.html \
    go.3 \
    go.html \
    go_mem.3 \
    go_mem.html \
    hclose.3 \
    hclose.html \
    hdone.3 \
    hdone.html \
    hdup.3 \
    hdup.html \
    hmake.3 \
    hmake.html \
    hquery.3 \
    hquery.html \
    msleep.3 \
    msleep.html \
    now.3 \
    now.html \
    yield.3 \
    yield.html \
    \
    brecv.3 \
    brecv.html \
    bsend.3 \
    bsend.html \
    crlf_attach.3 \
    crlf_attach.html \
    crlf_attach_mem.3 \
    crlf_attach_mem.html \
    crlf_detach.3 \
    crlf_detach.html \
    http_attach.3 \
    http_attach.html \
    http_attach_mem.3 \
    http_attach_mem.html \
    http_detach.3 \
    http_detach.html \
    http_recvfield.3 \
    http_recvfield.html \
    http_recvrequest.3 \
    http_recvrequest.html \
    http_recvstatus.3 \
    http_recvstatus.html \
    http_sendfield.3 \
    http_sendfield.html \
    http_sendrequest.3 \
    http_sendrequest.html \
    http_sendstatus.3 \
    http_sendstatus.html \
    ipaddr_family.3 \
    ipaddr_family.html \
    ipaddr_len.3 \
    ipaddr_len.html \
    ipaddr_local.3 \
    ipaddr_local.html \
    ipaddr_port.3 \
    ipaddr_port.html \
    ipaddr_remote.3 \
    ipaddr_remote.html \
    ipaddr_setport.3 \
    ipaddr_setport.html \
    ipaddr_sockaddr.3 \
    ipaddr_sockaddr.html \
    ipaddr_str.3 \
    ipaddr_str.html \
    ipc_accept.3 \
    ipc_accept.html \
    ipc_accept_mem.3 \
    ipc_accept_mem.html \
    ipc_close.3 \
    ipc_close.html \
    ipc_connect.3 \
    ipc_connect.html \
    ipc_connect_mem.3 \
    ipc_connect_mem.html \
    ipc_listen.3 \
    ipc_listen.html \
    ipc_listen_mem.3 \
    ipc_listen_mem.html \
    ipc_pair.3 \
    ipc_pair.html \
    ipc_pair_mem.3 \
    ipc_pair_mem.html \
    mrecv.3 \
    mrecv.html \
    msend.3 \
    msend.html \
    pfx_attach.3 \
    pfx_attach.html \
    pfx_attach_mem.3 \
    pfx_attach_mem.html \
    pfx_detach.3 \
    pfx_detach.html \
    tcp_accept.3 \
    tcp_accept.html \
    tcp_accept_mem.3 \
    tcp_accept_mem.html \
    tcp_close.3 \
    tcp_close.html \
    tcp_connect.3 \
    tcp_connect.html \
    tcp_connect_mem.3 \
    tcp_connect_mem.html \
    tcp_listen.3 \
    tcp_listen.html \
    tcp_listen_mem.3 \
    tcp_listen_mem.html \
    tls_attach_client.3 \
    tls_attach_client.html \
    tls_attach_client_mem.3 \
    tls_attach_client_mem.html \
    tls_attach_server.3 \
    tls_attach_server.html \
    tls_attach_server_mem.3 \
    tls_attach_server_mem.html \
    tls_detach.3 \
    tls_detach.html \
    udp_open.3 \
    udp_open.html \
    udp_open_mem.3 \
    udp_open_mem.html \
    udp_send.3 \
    udp_send.html \
    udp_sendl.3 \
    udp_sendl.html \
    udp_recv.3 \
    udp_recv.html \
    udp_recvl.3 \
    udp_recvl.html

SUFFIXES = .md .3 .html

man-local: $(man3_MANS)

# Compile the man page document
.md.3:
	$(AM_V_GEN) @PANDOC@ -s -f markdown_github -t man $< -o $@ \
		-M author="Martin Sustrik" \
		-M header="libdill Library Functions" \
		-M footer="libdill" \
		-M title="`echo "$(patsubst %.md,%,$<)(3)" | tr 'a-z' 'A-Z'`"

.md.html:
	$(AM_V_GEN) @PANDOC@ -f markdown_github -t html -o $@ $<

clean-local: clean-local-check

.PHONY: clean-local-check

clean-local-check:
	-rm $(man3_MANS)

EXTRA_DIST = $(man3_MANS)

