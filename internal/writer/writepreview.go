package writer

import (
	"bytes"
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"path"
	"strings"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/preview"
)

type Previewer = func(w io.Writer, blob *blobinfo, relpath int, param string) error

var Previewers = map[string]Previewer{
	"pandoc":     writepandoc,
	"configtree": writepreviewtree,
	"image":      writeimage,
}

var ErrInvalidParam = errors.New("invalid parameter")

func writepandoc(fp io.Writer, blob *blobinfo, relpath int, typ string) error {
	if typ == "" {
		return fmt.Errorf("%w: %s", ErrInvalidParam, typ)
	}
	var params = common.ExecuteParams{
		Command:     gitin.Config.Pandoccmd,
		CacheName:   "preview",
		Content:     blob.contents,
		Hash:        blob.hash,
		Destination: fp,
		Environ:     map[string]string{"filename": blob.name, "type": typ},
	}

	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	err := common.ExecuteCache(&params)
	fmt.Fprintf(fp, "</div>\n")

	return err
}

func writepreviewtree(fp io.Writer, blob *blobinfo, relpath int, typ string) error {
	if typ == "" {
		return fmt.Errorf("%w: %s", ErrInvalidParam, typ)
	}
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	err := preview.WriteConfigTree(fp, bytes.NewReader(blob.contents), typ)
	fmt.Fprintf(fp, "</div>\n")
	return err
}

func writeimage(fp io.Writer, blob *blobinfo, relpath int, filename string) error {
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	fmt.Fprintf(fp, "<img height=\"100px\" src=\"%sblob/%s\" />\n", strings.Repeat("../", relpath), filename)
	fmt.Fprintf(fp, "</div>\n")
	return nil
}

func writepreview(fp io.Writer, relpath int, blob *blobinfo) error {
	var typ string
	for _, ft := range gitin.Filetypes {
		if ft.Match(blob.name) {
			typ = ft.Preview
			break
		}
	}

	typ, param, _ := strings.Cut(typ, ":")
	if param == "" {
		param = strings.TrimLeft(path.Ext(blob.name), ".")
	}

	if fun, ok := Previewers[typ]; ok {
		err := fun(fp, blob, relpath, param)
		/* either nil or unknown error */
		if !errors.Is(err, ErrInvalidParam) {
			return nil
		}
	}

	fmt.Fprintf(fp, "<div class=\"preview\">\n<pre>\n")
	err := xml.EscapeText(fp, blob.contents)
	fmt.Fprintf(fp, "</pre>\n</div>\n")
	return err
}
