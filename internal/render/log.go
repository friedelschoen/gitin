package render

import (
	"encoding/json"
	"fmt"
	"html"
	"io"
	"strings"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
	git "github.com/jeffwelling/git2go/v37"
)

func writelogline(fp io.Writer, ci *wrapper.CommitInfo) {
	fmt.Fprint(fp, "<tr><td>")
	if ci.Author.Valid {
		fmt.Fprint(fp, rfc3339(ci.Author.When))
	}
	fmt.Fprint(fp, "</td><td>")
	if ci.Summary != "" {
		fmt.Fprintf(fp, "<a href=\"../commit/%s.html\">%s</a>", ci.ID, html.EscapeString(ci.Summary))
	}
	fmt.Fprintf(fp, "</td><td>")
	if ci.Author.Valid {
		fmt.Fprintf(fp, "%s", html.EscapeString(ci.Author.Name))
	}
	fmt.Fprintf(fp, "</td><td class=\"num\" align=\"right\">")
	fmt.Fprintf(fp, "%d", len(ci.Deltas))
	fmt.Fprintf(fp, "</td><td class=\"num\" align=\"right\">")
	fmt.Fprintf(fp, "+%d", ci.Addcount)
	fmt.Fprintf(fp, "</td><td class=\"num\" align=\"right\">")
	fmt.Fprintf(fp, "-%d", ci.Delcount)
	fmt.Fprint(fp, "</td></tr>\n")
}

// func writelogcommit(w io.Writer, info *wrapper.RepoInfo, index int, ci *wrapper.CommitInfo) error {
// 	pat := path.Join("commit", ci.ID+".html")

// 	commitfile, err := os.Create(pat)
// 	if err != nil {
// 		return err
// 	}
// 	defer commitfile.Close()
// 	if err := WriteCommit(commitfile, info, commit, ci, index == gitin.Config.Maxcommits); err != nil {
// 		return err
// 	}
// 	return nil
// }

func WriteLog(fp io.Writer, info *wrapper.RepoInfo, refname string, commits []*wrapper.CommitInfo, ncommits int, stats *LogStats, arsizes map[string]int64) error {
	WriteHeader(fp, info, 1, true, info.Name, refname)

	fmt.Fprintf(fp, "<h2>Archives</h2>")
	fmt.Fprintf(fp, "<table><thead>\n<tr><td class=\"expand\">Name</td>"+
		"<td class=\"num\">Size</td></tr>\n</thead><tbody>\n")

	for _, ext := range gitin.Config.Archives {
		ext = strings.TrimPrefix(ext, ".")
		arsize, ok := arsizes[ext]
		if !ok {
			continue
		}
		arsize, unit := common.Splitunit(arsize)
		fmt.Fprintf(fp, "<tr><td><a href=\"%s.%s\">%s.%s</a></td><td>%d%s</td></tr>", refname, ext, refname, ext, arsize, unit)
	}

	fmt.Fprintf(fp, "</tbody></table>")

	if err := stats.WriteOverview(fp); err != nil {
		return err
	}

	fmt.Fprintf(fp, "<h2>Commits of %s</h2>", refname)

	fmt.Fprintf(fp, "<table id=\"log\"><thead>\n<tr><td>Date</td>"+
		"<td class=\"expand\">Commit message</td>"+
		"<td>Author</td><td class=\"num\" align=\"right\">Files</td>"+
		"<td class=\"num\" align=\"right\">+</td>"+
		"<td class=\"num\" align=\"right\">-</td></tr>\n</thead><tbody>")

	msg := fmt.Sprintf("write log:   %s", refname)
	defer common.Printer.Done(msg)

	/* Iterate through the commits */
	for indx, commit := range commits {
		writelogline(fp, commit)

		common.Printer.Progress(msg, indx, len(commits))
	}

	if gitin.Config.Maxcommits > 0 && ncommits > gitin.Config.Maxcommits {
		var plural string
		if ncommits-gitin.Config.Maxcommits != 1 {
			plural = "s"
		}
		fmt.Fprintf(fp, "<tr><td></td><td colspan=\"5\">%d%s commits left out...</td></tr>\n", ncommits-gitin.Config.Maxcommits, plural)
	}

	fmt.Fprintf(fp, "</tbody></table>")

	WriteFooter(fp)
	return nil
}

func WriteLogJSON(fp io.Writer, refname string, commits []*wrapper.CommitInfo, ncommits int) error {
	/* Iterate through the commits */
	out := outJSON{
		Commits: commits,
	}
	enc := json.NewEncoder(fp)
	enc.SetIndent("", "  ")
	return enc.Encode(out)
}

func WriteLogAtom(atom io.Writer, info *wrapper.RepoInfo, refinfo *wrapper.ReferenceInfo) error {
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

	msg := fmt.Sprintf("write atoms: %s", refinfo.Refname)
	defer common.Printer.Done(msg)

	/* Iterate through the commits */
	indx := 0
	var commits []atomEntry
	_ = w.Iterate(func(commit *git.Commit) bool {
		commits = append(commits, makeEntry(commit, ""))
		indx++

		common.Printer.Progress(msg, indx, ncommits)
		return true
	})

	if err := writeatomfeed(atom, info, commits); err != nil {
		return err
	}
	return nil
}
