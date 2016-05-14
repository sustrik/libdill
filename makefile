%.html: %.md
	pandoc -f markdown_github $< > $@.tmp
	cat header.html $@.tmp footer.html > $@
	rm $@.tmp

all: index.html go.html

index.html: index.md
go.html: go.md
