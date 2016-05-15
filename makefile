%.html: %.md
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
    community.html \
    development.html \
    documentation.html \
    download.html \
    fdclean.html \
    fdin.html \
    fdout.html \
    go.html \
    handle.html \
    hclose.html \
    hdata.html \
    hdup.html \
    index.html \
    msleep.html \
    now.html \
    proc.html \
    setcls.html \
    yield.html

