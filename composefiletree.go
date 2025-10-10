package gitin

import (
	"fmt"
	"html"
	"io"
	"os"
	"path"
	"strings"

	git "github.com/jeffwelling/git2go/v37"
)

func filemode(m git.Filemode) string {
	return os.FileMode(m).String()
}

func highlight(fp io.Writer, blob *blobinfo) error {
	var typ string
	if idx := strings.LastIndexByte(blob.name, '.'); idx != -1 {
		typ = blob.name[idx+1:]
	} else {
		typ = "txt"
	}

	params := executeinfo{
		command:     Config.Highlightcmd,
		cachename:   "files",
		content:     blob.contents,
		contenthash: blob.hash,
		fp:          fp,
		environ:     map[string]string{"filename": blob.name, "type": typ, "scheme": Config.Colorscheme},
	}

	return execute(&params)
}

func writeblob(refname string, relpath int, blob *blobinfo) error {
	os.MkdirAll(".cache/blobs", 0777)
	destpath := path.Join(".cache/blobs", fmt.Sprintf("%x-%s", blob.hash, blob.name))
	if Force || !FileExist(destpath) {
		file, err := os.Create(destpath)
		if err != nil {
			return err
		}
		defer file.Close()
		file.Write(blob.contents)
	}

	hashpath := strings.Repeat("../", relpath) + destpath
	destpath = path.Join(refname, "blobs", blob.path)
	destpath = pathunhide(destpath)
	_ = os.Remove(destpath)
	if err := os.Symlink(hashpath, destpath); err != nil {
		return fmt.Errorf("unable to create symlink %s -> %s: %w", destpath, hashpath, err)
	}
	return nil
}

func writefile(info *repoinfo, refname string, relpath int, blob *blobinfo) error {
	os.MkdirAll(".cache/files", 0777)
	destpath := path.Join(".cache/files", fmt.Sprintf("%x-%s", blob.hash, blob.name))
	if Force || !FileExist(destpath) {
		fp, err := os.Create(destpath)
		if err != nil {
			return err
		}
		defer fp.Close()
		writeheader(fp, info, relpath, true, info.name, fmt.Sprintf("%s in %s", html.EscapeString(blob.path), refname))
		fmt.Fprintf(fp, "<p> %s (%dB) <a href='%sblob/%s/%s'>download</a></p><hr/>", html.EscapeString(blob.name),
			len(blob.contents), strings.Repeat("../", relpath), refname, pathunhide(blob.path))

		if err := writepreview(fp, relpath, blob); err != nil {
			return err
		}

		if blob.binary {
			fmt.Fprintf(fp, "<p>Binary file.</p>\n")
		} else if Config.Maxfilesize != -1 && len(blob.contents) >= Config.Maxfilesize {
			fmt.Fprintf(fp, "<p>File too big.</p>\n")
		} else if len(blob.contents) > 0 {
			if err := highlight(fp, blob); err != nil {
				return err
			}
		}

		writefooter(fp)
	}

	hashpath := strings.Repeat("../", relpath) + destpath
	destpath = path.Join(refname, "files", blob.path)
	destpath = pathunhide(destpath)
	_ = os.Remove(destpath)
	if err := os.Symlink(hashpath, destpath); err != nil {
		return fmt.Errorf("unable to create symlink %s -> %s: %w", destpath, hashpath, err)
	}
	return nil
}

func geticon(blob *git.Blob, filename string) string {
	for _, ft := range filetypes {
		if ft.Match(filename) {
			return ft.image
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

func writetree(fp io.Writer, info *repoinfo, refname string, baserelpath int,
	relpath int, tree *git.Tree, basepath string, index *uint64, maxfiles uint64) error {

	os.MkdirAll(path.Join(refname, "files", basepath), 0777)
	os.MkdirAll(path.Join(refname, "blobs", basepath), 0777)

	dosplit := Config.Splitdirectories
	if Config.SplitdirectoriesAuto {
		dosplit = maxfiles > uint64(Config.Autofilelimit)
	}

	var file *os.File
	if dosplit || basepath == "" {
		var err error
		file, err = os.Create(path.Join(refname, "files", basepath, "index.html"))
		if err != nil {
			return err
		}
		fp = file
		writeheader(fp, info, relpath, true, info.name, fmt.Sprintf("%s in %s", basepath, refname))

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

			obj, err := info.repo.LookupBlob(entry.Id)
			if err != nil {
				return err
			}
			fmt.Fprintf(fp, "<tr><td><img src=\"%sicons/%s.svg\" /></td><td>%s</td>\n",
				strings.Repeat("../", info.relpath+relpath), geticon(obj, entryname),
				filemode(entry.Filemode))

			var blob blobinfo
			blob.name = entryname
			blob.path = entrypath
			blob.binary = obj.IsBinary()
			blob.contents = obj.Contents()
			blob.hash = filehash(obj.Contents())

			if err := writefile(info, refname, relpath, &blob); err != nil {
				return err
			}
			if err := writeblob(refname, relpath, &blob); err != nil {
				return err
			}

			if dosplit {
				fmt.Fprintf(fp, "<td><a href=\"%s.html\">%s</a></td>", pathunhide(entryname), html.EscapeString(entryname))
			} else {
				fmt.Fprintf(fp, "<td><a href=\"%s.html\">%s</a></td>", pathunhide(entrypath), html.EscapeString(entrypath))
			}
			fmt.Fprintf(fp, "<td class=\"num\" align=\"right\">")
			fmt.Fprintf(fp, "%dB", obj.Size())
			fmt.Fprintf(fp, "</td></tr>\n")

			(*index)++

			printprogress(*index, maxfiles, fmt.Sprintf("write files: %-20s", refname))
		case git.ObjectTree:
			obj, err := info.repo.LookupTree(entry.Id)
			if err != nil {
				return err
			}
			if dosplit {
				fmt.Fprintf(
					fp,
					"<tr><td><img src=\"%sicons/directory.svg\" /></td><td>d---------</td><td colspan=\"2\"><a href=\"%s/\">%s</a></td></tr>\n",
					strings.Repeat("../", info.relpath+relpath), pathunhide(entrypath), html.EscapeString(entrypath))
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

func composefiletree(info *repoinfo, refinfo *referenceinfo) error {
	os.MkdirAll("cache/files", 0777)
	os.MkdirAll("cache/blobs", 0777)

	headoid := refinfo.commit.Id().String()
	if !Force {
		if file, err := os.Open(".cache/filetree"); err == nil {
			defer file.Close()
			b, err := io.ReadAll(file)
			if err == nil && string(b) == headoid {
				return nil
			}
		}
	}

	/* Clean /file and /blob directories since they will be rewritten */
	os.RemoveAll(path.Join(refinfo.refname, "files"))
	os.RemoveAll(path.Join(refinfo.refname, "blobs"))

	os.MkdirAll(path.Join(refinfo.refname, "files"), 0777)
	os.MkdirAll(path.Join(refinfo.refname, "blobs"), 0777)

	var indx uint64
	if tree, err := refinfo.commit.Tree(); err == nil {
		err = writetree(nil, info, refinfo.refname, 2, 2, tree, "", &indx, countfiles(tree))

		if err != nil {
			return err
		}

		if !Verbose && !Quiet {
			fmt.Println()
		}
	}

	if file, err := os.Create(".cache/filetree"); err == nil {
		defer file.Close()
		file.WriteString(headoid)
	}

	return nil
}
