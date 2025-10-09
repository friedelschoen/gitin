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

// #include "buffer.h"
// #include "common.h"
// #include "composer.h"
// #include "config.h"
// #include "execute.h"
// #include "getinfo.h"
// #include "fmt.Fprintf.h"
// #include "path.h"
// #include "writer.h"

// #include <git2/blob.h>
// #include <git2/commit.h>
// #include <limits.h>
// #include <string.h>
// #include <sys/stat.h>
// #include <unistd.h>

func filemode(m git.Filemode) string { // static const char* filemode(git.Filemode_t m) {
	return os.FileMode(m).String()
}

func highlight(fp io.Writer, blob *blobinfo) error { // static ssize_t highlight(io.Writer fp, struct blobinfo* blob) {
	var typ string
	if idx := strings.LastIndexByte(blob.name, '.'); idx != -1 { // if ((type = strrchr(blob->name, '.')) != NULL)
		typ = blob.name[idx+1:] // type++;
	} else {
		typ = "txt" // type = "txt";
	}

	params := executeinfo{ // struct executeinfo params = {
		command:     config.Highlightcmd,
		cachename:   "files",
		content:     blob.blob.Contents(), // .content     = git.Blob_rawcontent(blob->blob),
		contenthash: blob.hash,            // .contenthash = blob->hash,
		fp:          fp,
		environ:     map[string]string{"filename": blob.name, "type": typ, "scheme": config.Colorscheme}, // .environ = (const char*[]){ "filename", blob->name, "type", type, "scheme", colorscheme },
	} // };

	return execute(&params) // return execute(&params);
}

func writeblob(refname string, relpath int, blob *blobinfo) error { // static void writeblob(const char* refname, int relpath, struct blobinfo* blob) {
	os.MkdirAll(".cache/blobs", 0777)
	destpath := path.Join(".cache/blobs", fmt.Sprintf("%x-%s", blob.hash, blob.name))
	if force || !FileExist(destpath) {
		file, err := os.Create(destpath)
		if err != nil {
			return err
		}
		defer file.Close()
		file.Write(blob.blob.Contents())
	}

	// relpath :=
	// n = setrelpath(relpath, hashpath, unsafe.Sizeof(hashpath{}))                                  // n = setrelpath(relpath, hashpath, sizeof(hashpath));
	hashpath := strings.Repeat("../", relpath) + destpath
	destpath = path.Join(refname, "blobs", blob.path)
	destpath = pathunhide(destpath)                        // pathunhide(destpath);
	_ = os.Remove(destpath)                                // unlink(destpath);
	if err := os.Symlink(hashpath, destpath); err != nil { // if (symlink(hashpath, destpath))
		return fmt.Errorf("unable to create symlink %s -> %s: %w", destpath, hashpath, err) // fmt.Fprintf(stderr, "error: unable to create symlink %s -> %s\n", destpath, hashpath);
	}
	return nil
}

func writefile(info *repoinfo, refname string, relpath int, blob *blobinfo) error {
	os.MkdirAll(".cache/files", 0777)
	destpath := path.Join(".cache/files", fmt.Sprintf("%x-%s", blob.hash, blob.name))
	if force || !FileExist(destpath) {
		fp, err := os.Create(destpath)
		if err != nil {
			return err
		}
		defer fp.Close()
		writeheader(fp, info, relpath, true, info.name, fmt.Sprintf("%s in %s", html.EscapeString(blob.path), refname)) // writeheader(fp, info, relpath, 1, info->name, "%y in %s", blob->path, refname);
		fmt.Fprintf(fp, "<p> %s (%dB) <a href='%sblob/%s/%s'>download</a></p><hr/>", html.EscapeString(blob.name),      // fmt.Fprintf(fp, "<p> %y (%zuB) <a href='%rblob/%s/%h'>download</a></p><hr/>", blob->name,
			blob.blob.Size(), strings.Repeat("../", relpath), refname, pathunhide(blob.path)) // git.Blob_rawsize(blob->blob), relpath, refname, blob->path);

		if err := writepreview(fp, relpath, blob, 0); err != nil { // writepreview(fp, relpath, blob, 0);
			return err
		}

		if blob.blob.IsBinary() { // if (git.Blob_is_binary(blob->blob)) {
			fmt.Fprintf(fp, "<p>Binary file.</p>\n") // fmt.Fprintf("<p>Binary file.</p>\n", fp);
		} else if config.Maxfilesize != -1 && blob.blob.Size() >= int64(config.Maxfilesize) { // } else if (maxfilesize != -1 && git.Blob_rawsize(blob->blob) >= (size_t) maxfilesize) {
			fmt.Fprintf(fp, "<p>File too big.</p>\n") // fmt.Fprintf("<p>File too big.</p>\n", fp);
		} else if blob.blob.Size() > 0 { // } else if (git.Blob_rawsize(blob->blob) > 0) {
			if err := highlight(fp, blob); err != nil { // highlight(fp, blob);
				return err
			}
		}

		writefooter(fp) // writefooter(fp);
	}

	hashpath := strings.Repeat("../", relpath) + destpath
	destpath = path.Join(refname, "files", blob.path)
	destpath = pathunhide(destpath)                        // pathunhide(destpath);
	_ = os.Remove(destpath)                                // unlink(destpath);
	if err := os.Symlink(hashpath, destpath); err != nil { // if (symlink(hashpath, destpath))
		return fmt.Errorf("unable to create symlink %s -> %s: %w", destpath, hashpath, err) // fmt.Fprintf(stderr, "error: unable to create symlink %s -> %s\n", destpath, hashpath);
	}
	return nil
}

// func addheadfile(info *repoinfo, filename string) { // static void addheadfile(struct repoinfo* info, const char* filename) {
// 	/* Reallocate more space if needed */

// 	/* Copy the string and store it in the list */
// 	info.headfiles[info.headfileslen] = strdup(filename) // info->headfiles[info->headfileslen++] = strdup(filename);
// 	info.headfileslen++
// }

func geticon(blob *git.Blob, filename string) string { // static const char* geticon(const git.Blob* blob, const char* filename) {
	for _, ft := range filetypes {
		if ft.Match(filename) {
			return ft.image
		}
	}

	if blob.IsBinary() {
		return "binary"
	}
	return "other" // return git.Blob_is_binary(blob) ? "binary" : "other";
}

func countfiles(tree *git.Tree) (file_count uint64) { // static size_t countfiles(git.Repository* repo, git.Tree* tree) {
	_ = tree.Walk(func(s string, te *git.TreeEntry) error {
		if te.Type == git.ObjectBlob {
			file_count++
		}
		return nil
	})
	return
}

func writetree(fp io.Writer, info *repoinfo, refname string, baserelpath int, // static int writetree(io.Writer fp, const struct repoinfo* info, const char* refname, int baserelpath,
	relpath int, tree *git.Tree, basepath string, index *uint64, maxfiles uint64) error { // size_t maxfiles) {
	// var entry *git.Tree_entry = nil // const git.Tree_entry* entry = NULL;
	// var obj *git.Object = nil       // git.Object*           obj   = NULL;
	// var entryname string            // const char*           entryname;
	// var entrypath [PATH_MAX]byte
	// var oid [8]byte
	// var count uint64
	// var i uint64

	// var blob blobinfo // struct blobinfo       blob;
	// var dosplit int   // int                   dosplit;

	os.MkdirAll(path.Join(refname, "files", basepath), 0777)
	os.MkdirAll(path.Join(refname, "blobs", basepath), 0777)

	dosplit := config.Splitdirectories // dosplit = splitdirectories == -1 ? maxfiles > autofilelimit : splitdirectories;
	if config.SplitdirectoriesAuto {
		dosplit = maxfiles > uint64(config.Autofilelimit)
	}

	var file *os.File
	if dosplit || basepath == "" { // if (dosplit || !*basepath) {
		var err error
		file, err = os.Create(path.Join(refname, "files", basepath, "index.html")) // fp = efopen("w", "%s/files/%s/index.html", refname, basepath);
		if err != nil {
			return err
		}
		fp = file
		writeheader(fp, info, relpath, true, info.name, fmt.Sprintf("%s in %s", basepath, refname)) // writeheader(fp, info, relpath, 1, info->name, "%s in %s", basepath, refname);

		fmt.Fprint(fp, "<table id=\"files\"><thead>\n<tr><td></td><td>Mode</td><td class=\"expand\">Name</td><td class=\"num\" align=\"right\">Size</td></tr>\n</thead><tbody>\n") // fp);
	}

	for i := range tree.EntryCount() { // for (i = 0; i < count; i++) {
		entry := tree.EntryByIndex(i)
		entryname := entry.Name
		if entry == nil || entryname == "" {
			/* invalid entry */
			continue
		}
		entrypath := path.Join(basepath, entryname)

		switch entry.Type {
		case git.ObjectBlob:
			// info.headfiles = append(info.headfiles, entrypath)
			obj, err := info.repo.LookupBlob(entry.Id)
			if err != nil {
				return err
			}
			fmt.Fprintf(fp, "<tr><td><img src=\"%sicons/%s.svg\" /></td><td>%s</td>\n",
				strings.Repeat("../", info.relpath+relpath), geticon(obj, entryname), // info->relpath + relpath, geticon((git.Blob*) obj, entryname),
				filemode(entry.Filemode)) // filemode(git.Tree_entry_filemode(entry)));

			var blob blobinfo
			blob.name = entryname                // blob.name = entryname;
			blob.path = entrypath                // blob.path = entrypath;
			blob.blob = obj                      // blob.blob = (git.Blob*) obj;
			blob.hash = filehash(obj.Contents()) // git.Blob_rawsize((git.Blob*) obj));

			if err := writefile(info, refname, relpath, &blob); err != nil { // writefile(info, refname, relpath, &blob);
				return err
			}
			if err := writeblob(refname, relpath, &blob); err != nil { // writeblob(refname, relpath, &blob);
				return err
			}

			if dosplit { // if (dosplit)
				fmt.Fprintf(fp, "<td><a href=\"%s.html\">%s</a></td>", pathunhide(entryname), html.EscapeString(entryname)) // fmt.Fprintf(fp, "<td><a href=\"%h.html\">%y</a></td>", entryname, entryname);
			} else {
				fmt.Fprintf(fp, "<td><a href=\"%s.html\">%s</a></td>", pathunhide(entrypath), html.EscapeString(entrypath)) // fmt.Fprintf(fp, "<td><a href=\"%h.html\">%y</a></td>", entrypath, entrypath);
			}
			fmt.Fprintf(fp, "<td class=\"num\" align=\"right\">") // fmt.Fprintf("<td class=\"num\" align=\"right\">", fp);
			fmt.Fprintf(fp, "%dB", obj.Size())                    // fmt.Fprintf(fp, "%zuB", git.Blob_rawsize((git.Blob*) obj));
			fmt.Fprintf(fp, "</td></tr>\n")                       // fmt.Fprintf("</td></tr>\n", fp);

			(*index)++ // (*index)++;

			printprogress(*index, maxfiles, fmt.Sprintf("write files: %-20s", refname)) // printprogress(*index, maxfiles, "write files: %-20s", refname);
		case git.ObjectTree:
			obj, err := info.repo.LookupTree(entry.Id)
			if err != nil {
				return err
			}
			if dosplit {
				fmt.Fprintf(
					fp,
					"<tr><td><img src=\"%sicons/directory.svg\" /></td><td>d---------</td><td colspan=\"2\"><a href=\"%s/\">%s</a></td></tr>\n",
					strings.Repeat("../", info.relpath+relpath), pathunhide(entrypath), html.EscapeString(entrypath)) // info->relpath + relpath, entrypath, entrypath);
			}
			if err := writetree(fp, info, refname, baserelpath, relpath+1, obj, entrypath, index, maxfiles); err != nil { // index, maxfiles);
				return err
			}
		case git.ObjectCommit:
			fmt.Fprintf(
				fp,
				"<tr><td></td><td>m---------</td><td><a href=\"file/%s/-gitmodules.html\">%s</a>",
				refname, html.EscapeString(entrypath)) // refname, entrypath);
			fmt.Fprintf(fp, " @ %s</td><td class=\"num\" align=\"right\"></td></tr>\n", html.EscapeString(entry.Id.String())) // fmt.Fprintf(fp, " @ %y</td><td class=\"num\" align=\"right\"></td></tr>\n", oid);
		}
	}

	if file != nil { // if (dosplit || !*basepath) {
		fmt.Fprintf(fp, "</tbody></table>") // fmt.Fprintf("</tbody></table>", fp);
		writefooter(fp)                     // writefooter(fp);
		file.Close()                        // fclose(fp);
	}

	return nil
}

func composefiletree(info *repoinfo, refinfo *referenceinfo) error { // int composefiletree(const struct repoinfo* info, struct referenceinfo* refinfo) {
	os.MkdirAll("cache/files", 0777) // emkdirf("!.cache/files");
	os.MkdirAll("cache/blobs", 0777) // emkdirf("!.cache/files");

	headoid := refinfo.commit.Id().String()
	if !force { // if (!force) {
		if file, err := os.Open(".cache/filetree"); err == nil {
			defer file.Close()
			b, err := io.ReadAll(file)
			if err == nil && string(b) == headoid {
				return nil
			}
		}
	}

	/* Clean /file and /blob directories since they will be rewritten */
	os.RemoveAll(path.Join(refinfo.refname, "files")) // removedir(path);
	os.RemoveAll(path.Join(refinfo.refname, "blobs")) // removedir(path);

	os.MkdirAll(path.Join(refinfo.refname, "files"), 0777)
	os.MkdirAll(path.Join(refinfo.refname, "blobs"), 0777)

	var indx uint64
	if tree, err := refinfo.commit.Tree(); err == nil { // if (!git.Commit_tree(&tree, refinfo->commit)) {
		err = writetree(nil, info, refinfo.refname, 2, 2, tree, "", &indx, countfiles(tree)) // ret = writetree(NULL, info, refinfo->refname, 2, 2, tree, "", &indx,
		// countfiles(info->repo, tree));
		if err != nil {
			return err
		}

		if !verbose && !quiet { // if (!verbose && !quiet)
			fmt.Println()
		}
	}

	if file, err := os.Create(".cache/filetree"); err == nil {
		defer file.Close()
		file.WriteString(headoid)
	}

	return nil
}
