package preview

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
	git "github.com/jeffwelling/git2go/v37"
)

type Previewer = func(w io.Writer, filename string, blob *git.Blob, relpath int, param string) error

var ErrInvalidParam = errors.New("invalid parameter")

func DefaultPreviewer(w io.Writer, filename string, blob *git.Blob, relpath int, param string) error {
	fmt.Fprintf(w, "<div class=\"preview\">\n<pre>\n")
	err := xml.EscapeText(w, blob.Contents())
	fmt.Fprintf(w, "</pre>\n</div>\n")
	return err
}

func GetPreviewer(name string) (Previewer, string, bool) {
	for _, ft := range Filetypes {
		if ft.Match(name) {
			typ := ft.Preview
			param := ft.Param
			if param == "" {
				param = strings.TrimLeft(path.Ext(name), ".")
			}
			return typ, param, true
		}
	}
	return DefaultPreviewer, strings.TrimLeft(path.Ext(name), "."), false
}

func CommandPreviewer(cmdID string) Previewer {
	command := gitin.Config.PreviewCommands[cmdID]
	return func(w io.Writer, filename string, blob *git.Blob, relpath int, param string) error {
		if param == "" {
			return fmt.Errorf("%w: %s", ErrInvalidParam, param)
		}
		var params = common.ExecuteParams{
			Command:   command,
			CacheName: "preview-" + cmdID,
			Content:   blob.Contents(),
			ID:        blob.Id().String(),
			Environ:   map[string]string{"filename": filename, "type": param},
		}

		fmt.Fprintf(w, "<div class=\"preview\">\n")
		err := common.ExecuteCache(w, &params)
		fmt.Fprintf(w, "</div>\n")

		return err
	}
}

func ConfigPreviewer(fp io.Writer, filename string, blob *git.Blob, relpath int, typ string) error {
	if typ == "" {
		return fmt.Errorf("%w: %s", ErrInvalidParam, typ)
	}
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	err := WriteConfigTree(fp, bytes.NewReader(blob.Contents()), typ)
	fmt.Fprintf(fp, "</div>\n")
	return err
}

func ImagePreviewer(fp io.Writer, filename string, blob *git.Blob, relpath int, _ string) error {
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	fmt.Fprintf(fp, "<img height=\"100px\" src=\"%sblob/%s\" />\n", common.Relpath(relpath), filename)
	fmt.Fprintf(fp, "</div>\n")
	return nil
}
