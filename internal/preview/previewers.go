package preview

import (
	"bytes"
	"errors"
	"fmt"
	"io"
	"strings"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
)

type Previewer = func(w io.Writer, blob *wrapper.BlobInfo, relpath int, param string) error

var ErrInvalidParam = errors.New("invalid parameter")

func CommandPreviewer(cmdID string) Previewer {
	command := gitin.Config.PreviewCommands[cmdID]
	return func(w io.Writer, blob *wrapper.BlobInfo, relpath int, param string) error {
		if param == "" {
			return fmt.Errorf("%w: %s", ErrInvalidParam, param)
		}
		var params = common.ExecuteParams{
			Command:   command,
			CacheName: "preview-" + cmdID,
			Content:   blob.Contents,
			Hash:      blob.Hash,
			Environ:   map[string]string{"filename": blob.Name, "type": param},
		}

		fmt.Fprintf(w, "<div class=\"preview\">\n")
		err := common.ExecuteCache(w, &params)
		fmt.Fprintf(w, "</div>\n")

		return err
	}
}
func ConfigPreviewer(fp io.Writer, blob *wrapper.BlobInfo, relpath int, typ string) error {
	if typ == "" {
		return fmt.Errorf("%w: %s", ErrInvalidParam, typ)
	}
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	err := WriteConfigTree(fp, bytes.NewReader(blob.Contents), typ)
	fmt.Fprintf(fp, "</div>\n")
	return err
}

func ImagePreviewer(fp io.Writer, blob *wrapper.BlobInfo, relpath int, filename string) error {
	fmt.Fprintf(fp, "<div class=\"preview\">\n")
	fmt.Fprintf(fp, "<img height=\"100px\" src=\"%sblob/%s\" />\n", strings.Repeat("../", relpath), filename)
	fmt.Fprintf(fp, "</div>\n")
	return nil
}
