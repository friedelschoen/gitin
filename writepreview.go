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

func writepandoc(fp io.Writer, filename string, content []byte, contenthash uint32, typ string) error { // static ssize_t writepandoc(io.Writer fp, const char* filename, const char* content, ssize_t len,
	// uint32_t contenthash, const char* type) {

	var params = executeinfo{ // struct executeinfo params = {
		command:     config.Pandoccmd,
		cachename:   "preview",
		content:     content,
		contenthash: contenthash,
		fp:          fp,
		environ:     map[string]string{"filename": filename, "type": typ}, // .environ     = (const char*[]){ "filename", filename, "type", type },
	} // };

	fmt.Fprintf(fp, "<div class=\"preview\">\n") // fmt.Fprintf(fp, "<div class=\"preview\">\n");
	err := execute(&params)                      // ret = execute(&params);
	fmt.Fprintf(fp, "</div>\n")                  // fmt.Fprintf(fp, "</div>\n");

	return err // return ret;
}

func writepreviewtree(fp io.Writer, content []byte, typ string) { // static ssize_t writetree(io.Writer fp, const char* content, ssize_t len, uint32_t contenthash,
	fmt.Fprintf(fp, "<div class=\"preview\">\n") // fmt.Fprintf(fp, "<div class=\"preview\">\n");
	err := WriteConfigTree(fp, bytes.NewReader(content), typ)
	if err != nil {
		fmt.Fprint(os.Stdout, "error: ", err)
	}
	fmt.Fprintf(fp, "</div>\n") // fmt.Fprintf(fp, "</div>\n");
}

func writeimage(fp io.Writer, relpath int, filename string) { // static void writeimage(io.Writer fp, int relpath, const char* filename) {
	fmt.Fprintf(fp, "<div class=\"preview\">\n")                                                              // fmt.Fprintf(fp, "<div class=\"preview\">\n");
	fmt.Fprintf(fp, "<img height=\"100px\" src=\"%sblob/%s\" />\n", strings.Repeat("../", relpath), filename) // fmt.Fprintf(fp, "<img height=\"100px\" src=\"%rblob/%s\" />\n", relpath, filename);
	fmt.Fprintf(fp, "</div>\n")                                                                               // fmt.Fprintf(fp, "</div>\n");
}

func writeplain(fp io.Writer, content []byte) (err error) { // static void writeplain(io.Writer fp, const char* content, size_t size) {
	fmt.Fprintf(fp, "<div class=\"preview\">\n<pre>\n") // fmt.Fprintf(fp, "<div class=\"preview\">\n<pre>\n");
	err = xml.EscapeText(fp, content)
	fmt.Fprintf(fp, "</pre>\n</div>\n") // fmt.Fprintf(fp, "</pre>\n</div>\n");
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

func writepreview(fp io.Writer, relpath int, blob *blobinfo, printplain int) error { // void writepreview(io.Writer fp, int relpath, struct blobinfo* blob, int printplain) {
	var typ string
	for _, ft := range filetypes { // for (int i = 0; filetypes[i][0] != NULL; i++) {
		if ft.Match(blob.name) { // if (issuffix(blob->name, filetypes[i][0])) {
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

		if param != "" { // if (param)
			writepreviewtree(fp, blob.blob.Contents(), param) // param);
		}
	case "image":
		writeimage(fp, relpath, blob.name) // writeimage(fp, relpath, blob->name);
		return nil
	}
	return writeplain(fp, blob.blob.Contents())
}
