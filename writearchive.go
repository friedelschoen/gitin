package gitin

import (
	"archive/tar"
	"archive/zip"
	"compress/gzip"
	"fmt"
	"io"
	"path"
	"time"

	"github.com/dsnet/compress/bzip2"
	git "github.com/jeffwelling/git2go/v37"
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
		Method:        zip.Deflate, // gebruik deflate
		Modified:      hdr.ModTime,
		ExternalAttrs: uint32(hdr.Mode&0o777) << 16,
	}
	// UNIX perm-bits in upper 16 bits
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

	// Symlink detectie (0120000)
	const gitModeSymlink = 0o120000
	if int64(te.Filemode)&gitModeSymlink == gitModeSymlink {
		hdr.Typeflag = tar.TypeSymlink
		hdr.Linkname = string(blob.Contents())
		hdr.Size = 0 // symlinks hebben geen data payload in tar
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
func writearchive(fp io.Writer, info *repoinfo, typ ArchiveType, refinfo *referenceinfo) (int64, error) {
	fp, counter := NewCounter(fp)
	commit := refinfo.commit
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
		wc io.WriteCloser
		a  Archiver
	)
	switch typ {
	case ArchiveTarGz:
		wc = gzip.NewWriter(fp)
		a = tar.NewWriter(wc)
	case ArchiveTarXz:
		wc, err = xz.NewWriter(fp)
		if err != nil {
			return 0, err
		}
		a = tar.NewWriter(wc)
	case ArchiveTarBz2:
		wc, err = bzip2.NewWriter(fp, nil)
		if err != nil {
			return 0, err
		}
		a = tar.NewWriter(wc)
	case ArchiveZip:
		a = &ZipArchiver{Writer: zip.NewWriter(fp)}
	default:
		return 0, fmt.Errorf("invalid archive type: %d", typ)
	}

	// git2go v34: Tree.Walk(mode, cb) error. Gebruik Pre-order.
	err = tree.Walk(func(pfx string, te *git.TreeEntry) error {
		switch te.Type {
		case git.ObjectBlob:
			blob, err := info.repo.LookupBlob(te.Id)
			if err != nil {
				return fmt.Errorf("lookup blob %s: %w", te.Name, err)
			}
			name := te.Name
			if pfx != "" {
				name = path.Join(pfx, name) // altijd forward slashes
			}

			if err := writeArchiveBlob(blob, name, te, a, mtime); err != nil {
				return fmt.Errorf("write %s: %w", name, err)
			}
		}
		return nil
	})
	if err != nil {
		a.Close()
		if wc != nil {
			wc.Close()
		}
		return 0, fmt.Errorf("tree walk: %w", err)
	}

	if err := a.Close(); err != nil {
		return 0, err
	}
	if wc != nil {
		if err := wc.Close(); err != nil {
			return 0, err
		}
	}
	return *counter, nil
}
