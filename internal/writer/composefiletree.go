package writer

import (
	"fmt"
	"hash/crc32"
	"html"
	"io"
	"os"
	"path"
	"strings"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/preview"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
	git "github.com/jeffwelling/git2go/v37"
)

func filemode(m git.Filemode) string {
	return os.FileMode(m).String()
}

func writeblob(refname string, relpath int, blob *wrapper.BlobInfo) error {
	os.MkdirAll(".cache/blobs", 0777)
	destpath := path.Join(".cache/blobs", fmt.Sprintf("%x-%s", blob.Hash, blob.Name))
	if common.Force || !FileExist(destpath) {
		file, err := os.Create(destpath)
		if err != nil {
			return err
		}
		defer file.Close()
		file.Write(blob.Contents)
	}

	hashpath := strings.Repeat("../", relpath) + destpath
	destpath = path.Join(refname, "blobs", blob.Path)
	destpath = common.Pathunhide(destpath)
	_ = os.Remove(destpath)
	if err := os.Symlink(hashpath, destpath); err != nil {
		return fmt.Errorf("unable to create symlink %s -> %s: %w", destpath, hashpath, err)
	}
	return nil
}

func writefile(info *wrapper.RepoInfo, refname string, relpath int, blob *wrapper.BlobInfo) error {
	os.MkdirAll(".cache/files", 0777)
	destpath := path.Join(".cache/files", fmt.Sprintf("%x-%s", blob.Hash, blob.Name))
	if common.Force || !FileExist(destpath) {
		fp, err := os.Create(destpath)
		if err != nil {
			return err
		}
		defer fp.Close()
		writeheader(fp, info, relpath, true, info.Name, fmt.Sprintf("%s in %s", html.EscapeString(blob.Path), refname))
		fmt.Fprintf(fp, "<p> %s (%dB) <a href='%sblob/%s/%s'>download</a></p><hr/>", html.EscapeString(blob.Name),
			len(blob.Contents), strings.Repeat("../", relpath), refname, common.Pathunhide(blob.Path))

		if err := writepreview(fp, relpath, blob); err != nil {
			return err
		}

		if blob.IsBinary {
			fmt.Fprintf(fp, "<p>Binary file.</p>\n")
		} else if gitin.Config.Maxfilesize != -1 && len(blob.Contents) >= gitin.Config.Maxfilesize {
			fmt.Fprintf(fp, "<p>File too big.</p>\n")
		} else if len(blob.Contents) > 0 {
			if err := highlight(fp, blob); err != nil {
				return err
			}
		}

		writefooter(fp)
	}

	hashpath := strings.Repeat("../", relpath) + destpath
	destpath = path.Join(refname, "files", blob.Path)
	destpath = common.Pathunhide(destpath)
	_ = os.Remove(destpath)
	if err := os.Symlink(hashpath, destpath); err != nil {
		return fmt.Errorf("unable to create symlink %s -> %s: %w", destpath, hashpath, err)
	}
	return nil
}

func geticon(blob *git.Blob, filename string) string {
	for _, ft := range preview.Filetypes {
		if ft.Match(filename) {
			return ft.Image
		}
	}

	if blob.IsBinary() {
		return "binary"
	}
	return "other"
}

func countfiles(tree *git.Tree) (file_count uint64) {
	_ = tree.Walk(func(s string, te *git.TreeEntry) error {
		if te.Type == git.ObjectBlob {
			file_count++
		}
		return nil
	})
	return
}

func writetree(fp io.Writer, info *wrapper.RepoInfo, refname string, baserelpath int,
	relpath int, tree *git.Tree, basepath string, index *uint64, maxfiles uint64) error {

	os.MkdirAll(path.Join(refname, "files", basepath), 0777)
	os.MkdirAll(path.Join(refname, "blobs", basepath), 0777)

	dosplit := gitin.Config.Splitdirectories
	if gitin.Config.SplitdirectoriesAuto {
		dosplit = maxfiles > uint64(gitin.Config.Autofilelimit)
	}

	var file *os.File
	if dosplit || basepath == "" {
		var err error
		file, err = os.Create(path.Join(refname, "files", basepath, "index.html"))
		if err != nil {
			return err
		}
		fp = file
		writeheader(fp, info, relpath, true, info.Name, fmt.Sprintf("%s in %s", basepath, refname))

		fmt.Fprint(fp, "<table id=\"files\"><thead>\n<tr><td></td><td>Mode</td><td class=\"expand\">Name</td><td class=\"num\" align=\"right\">Size</td></tr>\n</thead><tbody>\n")
	}

	for i := range tree.EntryCount() {
		entry := tree.EntryByIndex(i)
		entryname := entry.Name
		if entryname == "" {
			/* invalid entry */
			continue
		}
		entrypath := path.Join(basepath, entryname)

		switch entry.Type {
		case git.ObjectBlob:

			obj, err := info.Repo.LookupBlob(entry.Id)
			if err != nil {
				return err
			}
			fmt.Fprintf(fp, "<tr><td><img src=\"%sicons/%s.svg\" /></td><td>%s</td>\n",
				strings.Repeat("../", info.Relpath+relpath), geticon(obj, entryname),
				filemode(entry.Filemode))

			blob := wrapper.BlobInfo{
				Name:     entryname,
				Path:     entrypath,
				IsBinary: obj.IsBinary(),
				Contents: obj.Contents(),
				Hash:     crc32.ChecksumIEEE(obj.Contents()),
			}

			if err := writefile(info, refname, relpath, &blob); err != nil {
				return err
			}
			if err := writeblob(refname, relpath, &blob); err != nil {
				return err
			}

			if dosplit {
				fmt.Fprintf(fp, "<td><a href=\"%s.html\">%s</a></td>", common.Pathunhide(entryname), html.EscapeString(entryname))
			} else {
				fmt.Fprintf(fp, "<td><a href=\"%s.html\">%s</a></td>", common.Pathunhide(entrypath), html.EscapeString(entrypath))
			}
			fmt.Fprintf(fp, "<td class=\"num\" align=\"right\">")
			fmt.Fprintf(fp, "%dB", obj.Size())
			fmt.Fprintf(fp, "</td></tr>\n")

			(*index)++

			common.Printprogress(*index, maxfiles, fmt.Sprintf("write files: %-20s", refname))
		case git.ObjectTree:
			obj, err := info.Repo.LookupTree(entry.Id)
			if err != nil {
				return err
			}
			if dosplit {
				fmt.Fprintf(
					fp,
					"<tr><td><img src=\"%sicons/directory.svg\" /></td><td>d---------</td><td colspan=\"2\"><a href=\"%s/\">%s</a></td></tr>\n",
					strings.Repeat("../", info.Relpath+relpath), common.Pathunhide(entrypath), html.EscapeString(entrypath))
			}
			if err := writetree(fp, info, refname, baserelpath, relpath+1, obj, entrypath, index, maxfiles); err != nil {
				return err
			}
		case git.ObjectCommit:
			fmt.Fprintf(
				fp,
				"<tr><td></td><td>m---------</td><td><a href=\"file/%s/-gitmodules.html\">%s</a>",
				refname, html.EscapeString(entrypath))
			fmt.Fprintf(fp, " @ %s</td><td class=\"num\" align=\"right\"></td></tr>\n", html.EscapeString(entry.Id.String()))
		}
	}

	if file != nil {
		fmt.Fprintf(fp, "</tbody></table>")
		writefooter(fp)
		file.Close()
	}

	return nil
}

func composefiletree(info *wrapper.RepoInfo, refinfo *wrapper.ReferenceInfo) error {
	os.MkdirAll("cache/files", 0777)
	os.MkdirAll("cache/blobs", 0777)

	headoid := refinfo.Commit.Id().String()
	if !common.Force {
		if file, err := os.Open(".cache/filetree"); err == nil {
			defer file.Close()
			b, err := io.ReadAll(file)
			if err == nil && string(b) == headoid {
				return nil
			}
		}
	}

	/* Clean /file and /blob directories since they will be rewritten */
	os.RemoveAll(path.Join(refinfo.Refname, "files"))
	os.RemoveAll(path.Join(refinfo.Refname, "blobs"))

	os.MkdirAll(path.Join(refinfo.Refname, "files"), 0777)
	os.MkdirAll(path.Join(refinfo.Refname, "blobs"), 0777)

	var indx uint64
	if tree, err := refinfo.Commit.Tree(); err == nil {
		err = writetree(nil, info, refinfo.Refname, 2, 2, tree, "", &indx, countfiles(tree))

		if err != nil {
			return err
		}

		if !common.Verbose && !common.Quiet {
			fmt.Println()
		}
	}

	if file, err := os.Create(".cache/filetree"); err == nil {
		defer file.Close()
		file.WriteString(headoid)
	}

	return nil
}
