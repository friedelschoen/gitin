package render

import (
	"encoding/xml"
	"errors"
	"fmt"
	"io"

	"github.com/friedelschoen/gitin-go/internal/preview"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
)

func WritePreview(fp io.Writer, relpath int, blob *wrapper.BlobInfo, typ preview.Previewer, param string) error {
	if typ != nil {
		err := typ(fp, blob, relpath, param)

		/* either nil or unknown error */
		if !errors.Is(err, preview.ErrInvalidParam) {
			return err
		}
	}

	/* fallback to just print the file */
	fmt.Fprintf(fp, "<div class=\"preview\">\n<pre>\n")
	err := xml.EscapeText(fp, blob.Contents)
	fmt.Fprintf(fp, "</pre>\n</div>\n")
	return err
}
