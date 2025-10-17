package render

import (
	"encoding/xml"
	"errors"
	"fmt"
	"io"

	"github.com/friedelschoen/gitin-go/internal/preview"
	git "github.com/jeffwelling/git2go/v37"
)

func WritePreview(fp io.Writer, relpath int, filename string, blob *git.Blob, typ preview.Previewer, param string) error {
	if typ != nil {
		err := typ(fp, filename, blob, relpath, param)

		/* either nil or unknown error */
		if !errors.Is(err, preview.ErrInvalidParam) {
			return err
		}
	}

	/* fallback to just print the file */
	fmt.Fprintf(fp, "<div class=\"preview\">\n<pre>\n")
	err := xml.EscapeText(fp, blob.Contents())
	fmt.Fprintf(fp, "</pre>\n</div>\n")
	return err
}
