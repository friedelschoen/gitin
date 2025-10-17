package render

import (
	"fmt"
	"io"

	"github.com/alecthomas/chroma/v2/formatters/html"
	"github.com/alecthomas/chroma/v2/lexers"
	"github.com/alecthomas/chroma/v2/styles"
	"github.com/friedelschoen/gitin-go"
	git "github.com/jeffwelling/git2go/v37"
)

func Highlight(w io.Writer, filename string, blob *git.Blob) error {
	lx := lexers.Get(filename)
	if lx == nil {
		lx = lexers.Fallback
	}

	st := styles.Get(gitin.Config.Colorscheme)
	if st == nil {
		st = styles.Fallback
	}

	iter, err := lx.Tokenise(nil, string(blob.Contents()))
	if err != nil {
		return fmt.Errorf("unable to tokenize: %w", err)
	}

	formatter := html.New(html.WithClasses(false), html.WithLineNumbers(true))
	return formatter.Format(w, st, iter)
}
