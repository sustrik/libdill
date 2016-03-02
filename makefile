index.html: index.md
	pandoc -f markdown_github index.md > index_.html
	cat header.html index_.html footer.html > index.html
	rm index_.html
