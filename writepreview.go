package gitin

import (
	"bytes"
	"encoding/xml"
	"fmt"
	"io"
	"os"
	"path"
	"strings"
)

func writepandoc(fp io.Writer, filename string, content []byte, contenthash uint32, typ string) error {

	var params = executeinfo{
		command:     config.Pandoccmd,
		cachename:   "preview",
		content:     content,
		contenthash: contenthash,
		fp:          fp,
		environ:     map[string]string{"filename": filename, "type": typ},
	}

	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	err := execute(&params)
	fmt.Fprintf(fp, "</div>\n")

	return err
}

func writepreviewtree(fp io.Writer, content []byte, typ string) {
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	err := WriteConfigTree(fp, bytes.NewReader(content), typ)
	if err != nil {
		fmt.Fprint(os.Stdout, "error: ", err)
	}
	fmt.Fprintf(fp, "</div>\n")
}

func writeimage(fp io.Writer, relpath int, filename string) {
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	fmt.Fprintf(fp, "<img height=\"100px\" src=\"%sblob/%s\" />\n", strings.Repeat("../", relpath), filename)
	fmt.Fprintf(fp, "</div>\n")
}

func writeplain(fp io.Writer, content []byte) (err error) {
	fmt.Fprintf(fp, "<div class=\"preview\">\n<pre>\n")
	err = xml.EscapeText(fp, content)
	fmt.Fprintf(fp, "</pre>\n</div>\n")
	return
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

func writepreview(fp io.Writer, relpath int, blob *blobinfo, printplain int) error {
	var typ string
	for _, ft := range filetypes {
		if ft.Match(blob.name) {
			typ = ft.preview
			break
		}
	}

	typ, param, _ := strings.Cut(typ, ":")

	switch typ {
	case "pandoc":
		if param == "" {
			param = strings.TrimLeft(path.Ext(blob.name), ".")
		}

		if param != "" {
			return writepandoc(fp, blob.name, blob.blob.Contents(), blob.hash, param)
		}
	case "configtree":
		if param == "" {
			param = strings.TrimLeft(path.Ext(blob.name), ".")
		}

		if param != "" {
			writepreviewtree(fp, blob.blob.Contents(), param)
		}
	case "image":
		writeimage(fp, relpath, blob.name)
		return nil
	}
	return writeplain(fp, blob.blob.Contents())
}
