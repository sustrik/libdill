#!/bin/sh

# Clean up the old man pages
git rm man/*

# Get the man page source files from the libdill repo.
git clone git@github.com:sustrik/libdill.git libdill-tmp
cd libdill-tmp
git checkout $1 --quiet
cd ..
cp libdill-tmp/man/*.md man
rm -rf libdill-tmp

# Build manpages
echo "" > manpage-index.md
for f in man/*.md
do
    # Manpage name.
    name=$(basename "$f")
    name="${name%.*}"
    # Build HTML version of manpage.
    pandoc -f markdown_github $f > $f.tmp
    cat header.html $f.tmp footer.html > man/$name.html
    rm $f $f.tmp
    # Add manpage to the index.
    echo "* [$name](http://libdill.org/man/$name.html)" >> manpage-index.md
done

# Build documenentation.md
cat documentation-header.md manpage-index.md documentation-footer.md > documentation.md
rm manpage-index.md

# Build other documentation
other="\
    build_options \
    compatibility \
    documentation \
    download \
    dsock-history \
    index \
    libdill-history \
    structured-concurrency \
    threads \
    tutorial"
for f in $other
do
    pandoc -f markdown_github $f.md > $f.tmp
    cat header.html $f.tmp footer.html > $f.html
    rm $f.tmp
done

