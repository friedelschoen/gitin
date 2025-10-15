package writer

import (
	"fmt"
	"io"
	"path"

	"github.com/alecthomas/chroma/v2/formatters/html"
	"github.com/alecthomas/chroma/v2/lexers"
	"github.com/alecthomas/chroma/v2/styles"
	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
)

func highlight(w io.Writer, blob *wrapper.BlobInfo) error {
	cachepath := path.Join(".cache/highlight", fmt.Sprintf("%s-%x", blob.Name, blob.Hash))
	return common.CachedWriter(w, cachepath, func(w io.Writer) error {
		lx := lexers.Get(blob.Name)
		if lx == nil {
			lx = lexers.Fallback
		}

		st := styles.Get(gitin.Config.Colorscheme)
		if st == nil {
			st = styles.Fallback
		}

		iter, err := lx.Tokenise(nil, string(blob.Contents))
		if err != nil {
			return fmt.Errorf("unable to tokenize: %w", err)
		}

		formatter := html.New(html.WithClasses(false), html.WithLineNumbers(true))
		return formatter.Format(w, st, iter)
	})
}
