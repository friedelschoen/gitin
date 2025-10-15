package writer

import (
	"errors"
	"fmt"
	"html"
	"io"
	"os"
	"path"
	"strings"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
	git "github.com/jeffwelling/git2go/v37"
)

func writelogline(fp io.Writer, commit *git.Commit, ci *wrapper.CommitInfo) {
	author := commit.Author()
	summary := commit.Summary()

	oid := commit.Id().String()

	fmt.Fprint(fp, "<tr><td>")
	if author != nil {
		fmt.Fprint(fp, rfc3339(author.When))
	}
	fmt.Fprint(fp, "</td><td>")
	if summary != "" {
		fmt.Fprintf(fp, "<a href=\"../commit/%s.html\">%s</a>", oid, html.EscapeString(summary))
	}
	fmt.Fprintf(fp, "</td><td>")
	if author != nil {
		fmt.Fprintf(fp, "%s", html.EscapeString(author.Name))
	}
	fmt.Fprintf(fp, "</td><td class=\"num\" align=\"right\">")
	fmt.Fprintf(fp, "%d", len(ci.Deltas))
	fmt.Fprintf(fp, "</td><td class=\"num\" align=\"right\">")
	fmt.Fprintf(fp, "+%d", ci.Addcount)
	fmt.Fprintf(fp, "</td><td class=\"num\" align=\"right\">")
	fmt.Fprintf(fp, "-%d", ci.Delcount)
	fmt.Fprint(fp, "</td></tr>\n")
}

func FileExist(pat string) bool {
	_, err := os.Stat(pat)
	return !errors.Is(err, os.ErrNotExist)
}

type logstate struct {
	fp   io.Writer
	json []commitJSON
	atom []atomEntry
}

func writelogcommit(s *logstate, info *wrapper.RepoInfo, index int, commit *git.Commit) error {
	pat := path.Join("commit", commit.Id().String()+".html")
	cachedcommit := !common.Force && FileExist(pat)
	ci, err := wrapper.GetDiff(info, commit, cachedcommit)
	if err != nil {
		return err
	}

	if !cachedcommit {
		commitfile, err := os.Create(pat)
		if err != nil {
			return err
		}
		defer commitfile.Close()
		if err := writecommit(commitfile, info, commit, &ci, index == gitin.Config.Maxcommits); err != nil {
			return err
		}
	}
	if s.fp != nil {
		writelogline(s.fp, commit, &ci)
	}
	s.atom = append(s.atom, makeEntry(commit, ""))
	s.json = append(s.json, toJSONCommit(commit))
	return nil
}

func writelog(fp io.Writer, atom io.Writer, json io.Writer, info *wrapper.RepoInfo, refinfo *wrapper.ReferenceInfo) error {

	os.Mkdir("commit", 0777)
	os.Mkdir(refinfo.Refname, 0777)

	if fp != nil {
		writeheader(fp, info, 1, true, info.Name, refinfo.Refname)

		fmt.Fprintf(fp, "<h2>Archives</h2>")
		fmt.Fprintf(fp, "<table><thead>\n<tr><td class=\"expand\">Name</td>"+
			"<td class=\"num\">Size</td></tr>\n</thead><tbody>\n")
	}

	for _, ext := range gitin.Config.Archives {
		ext = strings.TrimPrefix(ext, ".") /* .tar.xz -> tar.xz, .zip -> zip */
		arfp, err := os.Create(path.Join(refinfo.Refname, refinfo.Refname+"."+ext))
		if err != nil {
			return err
		}
		defer arfp.Close()
		arsize, err := writearchive(arfp, info, ext, refinfo)
		arsize, unit := common.Splitunit(arsize)
		if fp != nil {
			fmt.Fprintf(fp, "<tr><td><a href=\"%s.%s\">%s.%s</a></td><td>%d%s</td></tr>", refinfo.Refname, ext, refinfo.Refname, ext, arsize, unit)
		}
	}

	if fp != nil {
		fmt.Fprintf(fp, "</tbody></table>")

		if err := writeshortlog(fp, info, refinfo.Commit); err != nil {
			return err
		}

		fmt.Fprintf(fp, "<h2>Commits of %s</h2>", refinfo.Refname)

		fmt.Fprintf(fp, "<table id=\"log\"><thead>\n<tr><td>Date</td>"+
			"<td class=\"expand\">Commit message</td>"+
			"<td>Author</td><td class=\"num\" align=\"right\">Files</td>"+
			"<td class=\"num\" align=\"right\">+</td>"+
			"<td class=\"num\" align=\"right\">-</td></tr>\n</thead><tbody>")
	}

	/* Create a revwalk to iterate through the commits */
	w, err := info.Repo.Walk()
	if err != nil {
		return fmt.Errorf("unable to initialize revwalk: %w", err)
	}

	if err := w.Push(refinfo.Commit.Id()); err != nil {
		return err
	}
	ncommits := 0
	_ = w.Iterate(func(commit *git.Commit) bool {
		ncommits++
		return true
	})
	w.Reset()
	if err := w.Push(refinfo.Commit.Id()); err != nil {
		return err
	}

	msg := fmt.Sprintf("write log:   %s", refinfo.Refname)
	defer common.Printer.Done(msg)

	/* Iterate through the commits */
	var indx int = 0
	state := logstate{fp: fp}
	_ = w.Iterate(func(commit *git.Commit) bool {
		if gitin.Config.Maxcommits > 0 && indx >= gitin.Config.Maxcommits {
			return false
		}
		if err = writelogcommit(&state, info, indx, commit); err != nil {
			return false
		}
		indx++

		common.Printer.Progress(msg, indx, ncommits)
		return true
	})
	if err != nil {
		return err
	}
	if !common.Verbose && !common.Quiet {
		fmt.Println()
	}

	if fp != nil {
		if gitin.Config.Maxcommits > 0 && ncommits > gitin.Config.Maxcommits {
			var plural string
			if ncommits-gitin.Config.Maxcommits != 1 {
				plural = "s"
			}
			fmt.Fprintf(fp, "<tr><td></td><td colspan=\"5\">%d%s commits left out...</td></tr>\n", ncommits-gitin.Config.Maxcommits, plural)
		}

		fmt.Fprintf(fp, "</tbody></table>")
		writefooter(fp)
	}
	if json != nil {
		if err := writejsoncommits(json, state.json); err != nil {
			return err
		}
	}
	if atom != nil {
		if err := writeatomfeed(atom, info, state.atom); err != nil {
			return err
		}
	}
	return nil
}
