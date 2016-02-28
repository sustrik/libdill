index.html: index.md
	pandoc -f markdown_github header.html index.md footer.html > index.html
