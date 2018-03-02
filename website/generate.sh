#!/bin/sh

# Use this script to regenerate the website.

rm -f *.html
../man/manpages.src

# Build the man pages.
for f in *.md
do
    name=$(basename "$f")
    name="${name%.*}"
    echo "Processing ${name}"
    # Build HTML version of manpage.
    pandoc -f markdown_github $f > $name.tmp
    cat src/header.html $name.tmp src/footer.html > $name.html
    rm -f $name.tmp
done
rm -f toc.html

# Inject the function reference into the documentation page.
cat src/documentation.md.header toc.md src/documentation.md.footer \
    > src/documentation.md
rm -f *.md

# Build the remaining pages.
for f in src/*.md
do
    name=$(basename "$f")
    name="${name%.*}"
    echo "Processing ${name}"
    pandoc -f markdown_github -t html $f > $name.tmp
    cat src/header.html $name.tmp src/footer.html > $name.html
    rm -rf $name.tmp
done
rm -f src/documentation.md


