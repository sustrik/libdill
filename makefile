%.html: %.md header.html footer.html
	pandoc -f markdown_github $< > $@.tmp
	cat header.html $@.tmp footer.html > $@
	rm $@.tmp

all: \
    documentation.html \
    download.html \
    dsock-history.html \
    index.html \
    libdill-history.html \
    structured-concurrency.html \
    tutorial.html \
    \
    build_options.html \
    compatibility.html \
    threads.html \
    \
    channel.html \
    chdone.html \
    choose.html \
    chrecv.html \
    chsend.html \
    fdclean.html \
    fdin.html \
    fdout.html \
    go.html \
    go_stack.html \
    hclose.html \
    hdup.html \
    hmake.html \
    hquery.html \
    msleep.html \
    now.html \
    yield.html \

