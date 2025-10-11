package writer

import (
	"fmt"
	"hash/crc32"
	"io"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
	git "github.com/jeffwelling/git2go/v37"
)

func writesummary(fp io.Writer, info *wrapper.RepoInfo, refinfo *wrapper.ReferenceInfo) error {
	/* log for HEAD */
	writeheader(fp, info, 1, true, info.Name, refinfo.Refname)

	fmt.Fprintf(fp, "<div id=\"refcontainer\">")
	writerefs(fp, info, 1, refinfo.Ref)
	fmt.Fprintf(fp, "</div>")

	fmt.Fprintf(fp, "<hr />")

	if gitin.Config.Clonepull != "" {
		fmt.Fprintf(fp, "<h2>Clone</h2>")

		fmt.Fprintf(fp, "<i>Pulling</i>")
		fmt.Fprintf(fp, "<pre>git clone %s%s</pre>\n", gitin.Config.Clonepull, info.Repodir)

		if gitin.Config.Clonepush != "" {
			fmt.Fprintf(fp, "<i>Pushing</i>")
			if gitin.Config.Clonepull == gitin.Config.Clonepush {
				fmt.Fprintf(fp, "<pre>git push origin %s</pre>\n", refinfo.Refname)
			} else {
				fmt.Fprintf(fp, "<pre>git remote add my-remote %s%s\ngit push my-remote %s</pre>\n",
					gitin.Config.Clonepush, info.Repodir, refinfo.Refname)
			}
		}
	}

	var readme *git.Blob
	var readmename string
	for _, af := range gitin.Config.Aboutfiles {
		var err error
		readme, err = wrapper.GetCommitBlob(info.Repo, refinfo.Commit, readmename)
		readmename = af
		if err != nil && readme != nil {
			break
		}
	}

	if readme != nil {
		fmt.Fprintf(fp, "<h2>About</h2>\n")

		blobinfo := wrapper.BlobInfo{
			Name:     readmename,
			Path:     readmename,
			IsBinary: readme.IsBinary(),
			Contents: readme.Contents(),
			Hash:     crc32.ChecksumIEEE(readme.Contents()),
		}
		if err := writepreview(fp, 1, &blobinfo); err != nil {
			return err
		}
	}

	writefooter(fp)

	return nil
}
