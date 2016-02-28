index.html: index.md
	pandoc -f markdown header.html index.md footer.html > index.html
