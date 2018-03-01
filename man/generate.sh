#!/bin/sh

# Use this script to regenerate the man pages.

rm -f *.3
rm -f *.md
./manpages.src
for f in *.md
do
    echo "Processing $f"
	  pandoc -s -f markdown_github -t man $f -o "$(basename "$f" .md).3" \
		    -M header="libdill Library Functions" \
		    -M footer="libdill" \
		    -M title="`echo "$(basename "$f" .md)(3)" | tr 'a-z' 'A-Z'`"
done
rm -f *.md
