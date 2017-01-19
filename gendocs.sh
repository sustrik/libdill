#!/bin/sh

# Clean up the old man pages
git rm man/*.html
mkdir -p man

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
    cat templates/header.html $f.tmp templates/footer.html > man/$name.html
    rm $f $f.tmp
    git add man/$name.html
    # Add manpage to the index.
    echo "* [$name](http://libdill.org/man/$name.html)" >> manpage-index.md
done

# Build documenentation.md
cat templates/documentation-header.md manpage-index.md templates/documentation-footer.md > documentation.md
rm manpage-index.md

# Build other documentation
git rm *.html
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
    cat templates/header.html $f.tmp templates/footer.html > $f.html
    rm $f.tmp
    git add $f.html
done

rm documentation.md

