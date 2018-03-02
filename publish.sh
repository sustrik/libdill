#!/bin/sh

# Use this script to publish the website.

rm -f *.html
rm -f *.css
rm -f *.jpeg
rm -f *.png

git checkout master website/

cp website/*.html .
cp website/*.css .
cp website/*.jpeg .
cp website/*.png .

rm -rf website
