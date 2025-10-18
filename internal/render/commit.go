package render

import (
	"fmt"
	"html"
	"io"
	"slices"
	"strings"

	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
	git "github.com/jeffwelling/git2go/v37"
)

const MAXLINESTR = 80

func WriteCommit(fp io.Writer, info *wrapper.RepoInfo, ci *wrapper.CommitInfo) error {
	WriteHeader(fp, info, 1, false, info.Name, fmt.Sprintf("%s (%s)", html.EscapeString(ci.Summary), ci.ID))
	fmt.Fprintf(fp, "<pre>")

	fmt.Fprintf(fp, "<b>commit</b> <a href=\"%s.html\">%s</a>\n", ci.ID, ci.ID)

	if ci.ParentID != "" {
		if ci.Parent != nil {
			fmt.Fprintf(fp, "<b>parent</b> <a href=\"%s.html\">%s</a>\n", ci.ParentID, ci.ParentID)
		} else {
			fmt.Fprintf(fp, "<b>parent</b> %s\n", ci.ParentID)
		}
	}
	if ci.Author.Valid {
		fmt.Fprintf(fp, "<b>Author:</b> %s &lt;<a href=\"mailto:%s\">%s</a>&gt;\n<b>Date:</b>   %T\n",
			html.EscapeString(ci.Author.Name), html.EscapeString(ci.Author.Email), html.EscapeString(ci.Author.Email), rfc3339(ci.Author.When.UTC()))
	}
	if ci.Committer.Valid {
		fmt.Fprintf(fp, "<b>Committer:</b> %s &lt;<a href=\"mailto:%s\">%s</a>&gt;\n<b>Date:</b>   %T\n",
			html.EscapeString(ci.Committer.Name), html.EscapeString(ci.Committer.Email), html.EscapeString(ci.Committer.Email), rfc3339(ci.Committer.When.UTC()))
	}
	if ci.Message != "" {
		fmt.Fprintf(fp, "\n%s\n", html.EscapeString(ci.Message))
	}
	if len(ci.Deltas) == 0 {
		return nil
	}
	if len(ci.Deltas) > 1000 || ci.Addcount > 100000 || ci.Delcount > 100000 {
		fmt.Fprintf(fp, "Diff is too large, output suppressed.\n")
		return nil
	}

	/* diff stat */
	fmt.Fprintf(fp, "<b>Diffstat:</b>\n<table>")
	for i, del := range ci.Deltas {
		delta := del.Delta

		var c rune
		switch delta.Status {
		case git.DeltaAdded:
			c = 'A'
		case git.DeltaCopied:
			c = 'C'
		case git.DeltaDeleted:
			c = 'D'
		case git.DeltaModified:
			c = 'M'
		case git.DeltaRenamed:
			c = 'R'
		case git.DeltaTypeChange:
			c = 'T'
		}
		if c == 0 {
			fmt.Fprintf(fp, "<tr><td> ")
		} else {
			fmt.Fprintf(fp, "<tr><td class=\"%c\">%c", c, c)
		}
		fmt.Fprintf(fp, "</td><td><a href=\"#h%d\">%s", i, html.EscapeString(delta.OldFile.Path))
		if delta.OldFile.Path != delta.NewFile.Path {
			fmt.Fprintf(fp, " -&gt; \n%s", html.EscapeString(delta.NewFile.Path))
		}
		add := del.Addcount
		deletion := del.Delcount

		changed := add + deletion
		if changed > MAXLINESTR {
			if add > 0 {
				add = int(float64(MAXLINESTR*add)/float64(changed)) + 1
			}
			if deletion > 0 {
				deletion = int(float64(MAXLINESTR*deletion)/float64(changed)) + 1
			}
		}

		fmt.Fprintf(fp, "</a></td><td class=\"num\">%d</td><td class=\"expand\"><span class=\"i\">%s</span><span class=\"d\">%s</span></td></tr>\n",
			del.Addcount+del.Delcount, strings.Repeat("+", add), strings.Repeat("-", deletion))
	}
	var plFile, plAdd, plDel string
	if len(ci.Deltas) > 0 {
		plFile = "s"
	}
	if ci.Addcount > 0 {
		plAdd = "s"
	}
	if ci.Delcount > 0 {
		plDel = "s"
	}
	fmt.Fprintf(fp, "</table></pre><pre>%d file%s changed, %d insertion%s(+), %d deletion%s(-)\n",
		len(ci.Deltas), plFile, ci.Addcount, plAdd, ci.Delcount, plDel)

	fmt.Fprintf(fp, "<hr/>")

	for i, del := range ci.Deltas {
		delta := del.Delta

		if slices.Contains(info.Headfiles, delta.OldFile.Path) {
			fmt.Fprintf(fp, "<b>diff --git a/<a id=\"h%d\" href=\"../file/%s.html\">%s</a> ", i,
				common.Pathunhide(delta.OldFile.Path), html.EscapeString(delta.OldFile.Path))
		} else {
			fmt.Fprintf(fp, "<b>diff --git a/%s ", html.EscapeString(delta.OldFile.Path))
		}
		if slices.Contains(info.Headfiles, delta.NewFile.Path) {
			fmt.Fprintf(fp, "b/<a href=\"../file/%s.html\">%s</a></b>\n", common.Pathunhide(delta.NewFile.Path), html.EscapeString(delta.NewFile.Path))
		} else {
			fmt.Fprintf(fp, "b/%s</b>\n", html.EscapeString(delta.NewFile.Path))
		}
		if delta.Flags&git.DiffFlagBinary > 0 {
			fmt.Fprintf(fp, "Binary files differ.\n")
			continue
		}

		for _, hunks := range ci.Hunks {
			for j, hunk := range *hunks {
				fmt.Fprintf(fp, "<a href=\"#h%d-%d\" id=\"h%d-%d\" class=\"h\">%s</a>\n", i, j, i, j, html.EscapeString(hunk.Header))

				for k, line := range hunk.Changes {
					if line.OldLine == -1 {
						fmt.Fprintf(fp, "<a href=\"#h%d-%d-%d\" id=\"h%d-%d-%d\" class=\"i\">+", i, j, k, i, j, k)
					} else if line.NewLine == -1 {
						fmt.Fprintf(fp, "<a href=\"#h%d-%d-%d\" id=\"h%d-%d-%d\" class=\"d\">-", i, j, k, i, j, k)
					} else {
						fmt.Fprintf(fp, " ")
					}
					fmt.Fprintf(fp, "%s\n", html.EscapeString(line.Content))
					if line.OldLine == -1 || line.NewLine == -1 {
						fmt.Fprintf(fp, "</a>")
					}
				}
			}
		}
	}

	fmt.Fprintf(fp, "</pre>\n")
	WriteFooter(fp)
	return nil
}
