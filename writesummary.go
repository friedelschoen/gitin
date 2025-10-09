package gitin

import (
	"errors"
	"fmt"
	"hash/crc32"
	"io"

	git "github.com/jeffwelling/git2go/v37"
)

// #include "common.h"
// #include "config.h"
// #include "getinfo.h"
// #include "writer.h"

// #include <git2/blob.h>
// #include <git2/commit.h>
// #include <git2/tree.h>
// #include <git2/types.h>
// #include <stdio.h>
// #include <string.h>

var ErrNoBlob = errors.New("object not a blob")

func getcommitblob(repo *git.Repository, commit *git.Commit, path string) (*git.Blob, error) { // static git.Blob* getcommitblob(const git.Commit* commit, const char* path) {
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

func writesummary(fp io.Writer, info *repoinfo, refinfo *referenceinfo) error { // int writesummary(io.Writer fp, const struct repoinfo* info, struct referenceinfo* refinfo) {
	// var readmename string      // const char*     readmename;
	// var readme *git.Blob = nil // git.Blob*       readme = NULL;
	// var blobinfo blobinfo      // struct blobinfo blobinfo;

	/* log for HEAD */
	writeheader(fp, info, 1, true, info.name, refinfo.refname) // writeheader(fp, info, 1, 1, info->name, "%s", refinfo->refname);

	fmt.Fprintf(fp, "<div id=\"refcontainer\">") // fmt.Fprintf(fp, "<div id=\"refcontainer\">");
	writerefs(fp, info, 1, refinfo.ref)          // writerefs(fp, info, 1, refinfo->ref);
	fmt.Fprintf(fp, "</div>")                    // fmt.Fprintf(fp, "</div>");

	fmt.Fprintf(fp, "<hr />") // fmt.Fprintf("<hr />", fp);

	if config.Clonepull != "" { // if (clonepull) {
		fmt.Fprintf(fp, "<h2>Clone</h2>") // fmt.Fprintf(fp, "<h2>Clone</h2>");

		fmt.Fprintf(fp, "<i>Pulling</i>")                                              // fmt.Fprintf(fp, "<i>Pulling</i>");
		fmt.Fprintf(fp, "<pre>git clone %s%s</pre>\n", config.Clonepull, info.repodir) // fmt.Fprintf(fp, "<pre>git clone %s%s</pre>\n", clonepull, info->repodir);

		if config.Clonepush != "" { // if (clonepush) {
			fmt.Fprintf(fp, "<i>Pushing</i>")         // fmt.Fprintf(fp, "<i>Pushing</i>");
			if config.Clonepull == config.Clonepush { // if (!strcmp(clonepull, clonepush)) {
				fmt.Fprintf(fp, "<pre>git push origin %s</pre>\n", refinfo.refname) // fmt.Fprintf(fp, "<pre>git push origin %s</pre>\n", refinfo->refname);
			} else {
				fmt.Fprintf(fp, "<pre>git remote add my-remote %s%s\ngit push my-remote %s</pre>\n",
					config.Clonepush, info.repodir, refinfo.refname) // clonepush, info->repodir, refinfo->refname);
			}
		}
	}

	var readme *git.Blob
	var readmename string
	for _, af := range aboutfiles { // for (int i = 0; aboutfiles[i]; i++) {
		var err error
		readme, err = getcommitblob(info.repo, refinfo.commit, readmename)
		readmename = af
		if err != nil && readme != nil { // if ((readme = getcommitblob(refinfo->commit, aboutfiles[i])))
			break // break;
		}
	}

	if readme != nil { // if (readme) {
		fmt.Fprintf(fp, "<h2>About</h2>\n") // fmt.Fprintf(fp, "<h2>About</h2>\n");

		var blobinfo blobinfo
		blobinfo.name = readmename                                // blobinfo.name = readmename;
		blobinfo.path = readmename                                // blobinfo.path = readmename;
		blobinfo.blob = readme                                    // blobinfo.blob = readme;
		blobinfo.hash = filehash(readme.Contents())               // blobinfo.hash = filehash(git.Blob_rawcontent(readme), git.Blob_rawsize(readme));
		if err := writepreview(fp, 1, &blobinfo, 1); err != nil { // writepreview(fp, 1, &blobinfo, 1);
			return err
		}
	}

	writefooter(fp) // writefooter(fp);

	return nil // return 0;
}
