package gitin

import (
	"errors"
	"fmt"
	"hash/crc32"
	"io"

	git "github.com/jeffwelling/git2go/v37"
)

var ErrNoBlob = errors.New("object not a blob")

func getcommitblob(repo *git.Repository, commit *git.Commit, path string) (*git.Blob, error) {
	tree, err := commit.Tree()
	if err != nil {
		return nil, err
	}

	entry, err := tree.EntryByPath(path)
	if err != nil {
		return nil, err
	}

	if entry.Type != git.ObjectBlob {
		return nil, ErrNoBlob
	}

	return repo.LookupBlob(entry.Id)
}

func filehash(b []byte) uint32 {
	return crc32.ChecksumIEEE(b)
}

func writesummary(fp io.Writer, info *repoinfo, refinfo *referenceinfo) error {

	/* log for HEAD */
	writeheader(fp, info, 1, true, info.name, refinfo.refname)

	fmt.Fprintf(fp, "<div id=\"refcontainer\">")
	writerefs(fp, info, 1, refinfo.ref)
	fmt.Fprintf(fp, "</div>")

	fmt.Fprintf(fp, "<hr />")

	if Config.Clonepull != "" {
		fmt.Fprintf(fp, "<h2>Clone</h2>")

		fmt.Fprintf(fp, "<i>Pulling</i>")
		fmt.Fprintf(fp, "<pre>git clone %s%s</pre>\n", Config.Clonepull, info.repodir)

		if Config.Clonepush != "" {
			fmt.Fprintf(fp, "<i>Pushing</i>")
			if Config.Clonepull == Config.Clonepush {
				fmt.Fprintf(fp, "<pre>git push origin %s</pre>\n", refinfo.refname)
			} else {
				fmt.Fprintf(fp, "<pre>git remote add my-remote %s%s\ngit push my-remote %s</pre>\n",
					Config.Clonepush, info.repodir, refinfo.refname)
			}
		}
	}

	var readme *git.Blob
	var readmename string
	for _, af := range aboutfiles {
		var err error
		readme, err = getcommitblob(info.repo, refinfo.commit, readmename)
		readmename = af
		if err != nil && readme != nil {
			break
		}
	}

	if readme != nil {
		fmt.Fprintf(fp, "<h2>About</h2>\n")

		var blobinfo blobinfo
		blobinfo.name = readmename
		blobinfo.path = readmename
		blobinfo.blob = readme
		blobinfo.hash = filehash(readme.Contents())
		if err := writepreview(fp, 1, &blobinfo, 1); err != nil {
			return err
		}
	}

	writefooter(fp)

	return nil
}
