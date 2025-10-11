package writer

import (
	"archive/tar"
	"archive/zip"
	"compress/gzip"
	"fmt"
	"io"
	"path"
	"strings"
	"time"

	"github.com/dsnet/compress/bzip2"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
	git "github.com/jeffwelling/git2go/v37"
	"github.com/klauspost/compress/zstd"
	"github.com/ulikunitz/xz"
)

type Archiver interface {
	io.WriteCloser
	WriteHeader(hdr *tar.Header) error
	Write([]byte) (int, error)
}

type ZipArchiver struct {
	*zip.Writer
	cur io.Writer
}

func (a *ZipArchiver) WriteHeader(hdr *tar.Header) error {
	fh := &zip.FileHeader{
		Name:          hdr.Name,
		Method:        zip.Deflate,
		Modified:      hdr.ModTime,
		ExternalAttrs: uint32(hdr.Mode&0o777) << 16,
	}

	w, err := a.CreateHeader(fh)
	if err != nil {
		return err
	}
	a.cur = w
	return nil
}

func (a *ZipArchiver) Write(b []byte) (int, error) {
	if a.cur == nil {
		return 0, io.ErrClosedPipe
	}
	return a.cur.Write(b)
}

/*  Schrijf één blob naar het archief (regulier bestand of symlink) */
func writeArchiveBlob(blob *git.Blob, pathname string, te *git.TreeEntry, a Archiver, mtime time.Time) error {
	mode := int64(te.Filemode) & 0o777

	hdr := tar.Header{
		Name:    pathname,
		Mode:    mode,
		Size:    blob.Size(),
		ModTime: mtime,
	}

	const gitModeSymlink = 0o120000
	if int64(te.Filemode)&gitModeSymlink == gitModeSymlink {
		hdr.Typeflag = tar.TypeSymlink
		hdr.Linkname = string(blob.Contents())
		hdr.Size = 0
	}

	if err := a.WriteHeader(&hdr); err != nil {
		return err
	}
	if hdr.Typeflag != tar.TypeSymlink {
		_, err := a.Write(blob.Contents())
		return err
	}
	return nil
}

type counter struct {
	w io.Writer
	c int64
}

func (c *counter) Write(b []byte) (int, error) {
	n, err := c.w.Write(b)
	c.c += int64(n)
	return n, err
}

func NewCounter(w io.Writer) (io.Writer, *int64) {
	c := counter{w: w}
	return &c, &c.c
}

/* Stream een archief van de commit-tree naar fp */
func writearchive(w io.Writer, info *wrapper.RepoInfo, ext string, refinfo *wrapper.ReferenceInfo) (int64, error) {
	w, counter := NewCounter(w)
	commit := refinfo.Commit
	committer := commit.Committer()
	mtime := time.Now().UTC()
	if committer != nil && !committer.When.IsZero() {
		mtime = committer.When.UTC()
	}

	tree, err := commit.Tree()
	if err != nil {
		return 0, fmt.Errorf("get tree: %w", err)
	}

	var (
		cls io.Closer = io.NopCloser(nil)
		a   Archiver
	)
	extarchive, extcompress, _ := strings.Cut(ext, ".")
	switch extcompress {
	case "gz":
		gw := gzip.NewWriter(w)
		cls = gw
		w = gw
	case "xz":
		gw, err := xz.NewWriter(w)
		if err != nil {
			return 0, err
		}
		cls = gw
		w = gw
	case "bzip2", "b2":
		gw, err := bzip2.NewWriter(w, nil)
		if err != nil {
			return 0, err
		}
		cls = gw
		w = gw
	case "zstd":
		gw, err := zstd.NewWriter(w, nil)
		if err != nil {
			return 0, err
		}
		cls = gw
		w = gw
	case "":
		/* do nothing */
	default:
		return 0, fmt.Errorf("unknown compression: %s", extcompress)
	}
	/* defers are closed in Last-In-First-Out order */
	defer cls.Close()

	switch extarchive {
	case "tar":
		a = tar.NewWriter(w)
	case "zip":
		a = &ZipArchiver{Writer: zip.NewWriter(w)}
	default:
		return 0, fmt.Errorf("unknown archiver: %s", extcompress)
	}
	defer a.Close()

	err = tree.Walk(func(pfx string, te *git.TreeEntry) error {
		switch te.Type {
		case git.ObjectBlob:
			blob, err := info.Repo.LookupBlob(te.Id)
			if err != nil {
				return fmt.Errorf("lookup blob %s: %w", te.Name, err)
			}
			name := te.Name
			if pfx != "" {
				name = path.Join(pfx, name)
			}

			if err := writeArchiveBlob(blob, name, te, a, mtime); err != nil {
				return fmt.Errorf("write %s: %w", name, err)
			}
		}
		return nil
	})
	if err != nil {
		return 0, fmt.Errorf("tree walk: %w", err)
	}
	return *counter, nil
}
