package gitin

import (
	"bytes"
	"encoding/xml"
	"errors"
	"fmt"
	"io"
	"path"
	"strings"
)

type Previewer = func(w io.Writer, blob *blobinfo, relpath int, param string) error

var ErrInvalidParam = errors.New("invalid parameter")

func writepandoc(fp io.Writer, blob *blobinfo, relpath int, typ string) error {
	if typ == "" {
		return fmt.Errorf("%w: %s", ErrInvalidParam, typ)
	}
	var params = executeinfo{
		command:     Config.Pandoccmd,
		cachename:   "preview",
		content:     blob.contents,
		contenthash: blob.hash,
		fp:          fp,
		environ:     map[string]string{"filename": blob.name, "type": typ},
	}

	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	err := execute(&params)
	fmt.Fprintf(fp, "</div>\n")

	return err
}

func writepreviewtree(fp io.Writer, blob *blobinfo, relpath int, typ string) error {
	if typ == "" {
		return fmt.Errorf("%w: %s", ErrInvalidParam, typ)
	}
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	err := WriteConfigTree(fp, bytes.NewReader(blob.contents), typ)
	fmt.Fprintf(fp, "</div>\n")
	return err
}

func writeimage(fp io.Writer, blob *blobinfo, relpath int, filename string) error {
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	fmt.Fprintf(fp, "<img height=\"100px\" src=\"%sblob/%s\" />\n", strings.Repeat("../", relpath), filename)
	fmt.Fprintf(fp, "</div>\n")
	return nil
}

var Previewers = map[string]Previewer{
	"pandoc":     writepandoc,
	"configtree": writepreviewtree,
	"image":      writeimage,
}

type FileType struct {
	pattern string
	image   string
	preview string
}

func (ft FileType) Match(s string) bool {
	pre, suf, ok := strings.Cut(ft.pattern, "*")
	if !ok {
		return s == ft.pattern
	}
	return strings.HasPrefix(s, pre) && strings.HasSuffix(s, suf)
}

func writepreview(fp io.Writer, relpath int, blob *blobinfo) error {
	var typ string
	for _, ft := range filetypes {
		if ft.Match(blob.name) {
			typ = ft.preview
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
