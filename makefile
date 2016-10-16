%.html: %.md header.html footer.html
	pandoc -f markdown_github $< > $@.tmp
	cat header.html $@.tmp footer.html > $@
	rm $@.tmp

all: \
    channel.html \
    chdone.html \
    choose.html \
    chrecv.html \
    chsend.html \
    cls.html \
    documentation.html \
    download.html \
    fdclean.html \
    fdin.html \
    fdout.html \
    go.html \
    handle.html \
    hclose.html \
    hdup.html \
    hquery.html \
    index.html \
    msleep.html \
    now.html \
    proc.html \
    setcls.html \
    structured-concurrency.html \
    yield.html \
    \
    blog.html \
    bsock.html \
    bthrottler.html \
    crlf.html \
    ipaddr.html \
    keepalive.html \
    lz4.html \
    mlog.html \
    msock.html \
    mthrottler.html \
    nacl.html \
    nagle.html \
    pfx.html \
    tcp.html \
    udp.html \
    unix.html 

