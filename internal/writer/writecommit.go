package writer

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

func hasheadfile(info *wrapper.RepoInfo, filename string) bool {
	return slices.Contains(info.Headfiles, filename)
}

func writecommit(fp io.Writer, info *wrapper.RepoInfo, commit *git.Commit, ci *wrapper.CommitInfo, parentlink bool) error {

	author := commit.Author()
	summary := commit.Summary()
	msg := commit.Message()
	oid := commit.Id().String()
	var parentoid string
	if parentcommit := commit.ParentId(0); parentcommit != nil {
		parentoid = parentcommit.String()
	}

	writeheader(fp, info, 1, false, info.Name, fmt.Sprintf("%s (%s)", html.EscapeString(summary), oid))
	fmt.Fprintf(fp, "<pre>")

	fmt.Fprintf(fp, "<b>commit</b> <a href=\"%s.html\">%s</a>\n", oid, oid)

	if parentoid != "" {
		if parentlink {
			fmt.Fprintf(fp, "<b>parent</b> <a href=\"%s.html\">%s</a>\n", parentoid, parentoid)
		} else {
			fmt.Fprintf(fp, "<b>parent</b> %s\n", parentoid)
		}
	}
	if author != nil {
		fmt.Fprintf(fp, "<b>Author:</b> %s &lt;<a href=\"mailto:%s\">%s</a>&gt;\n<b>Date:</b>   %T\n",
			html.EscapeString(author.Name), html.EscapeString(author.Email), html.EscapeString(author.Email), rfc3339(author.When.UTC()))
	}
	if msg != "" {
		fmt.Fprintf(fp, "\n%s\n", html.EscapeString(msg))
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

		if hasheadfile(info, delta.OldFile.Path) {
			fmt.Fprintf(fp, "<b>diff --git a/<a id=\"h%d\" href=\"../file/%s.html\">%s</a> ", i,
				common.Pathunhide(delta.OldFile.Path), html.EscapeString(delta.OldFile.Path))
		} else {
			fmt.Fprintf(fp, "<b>diff --git a/%s ", html.EscapeString(delta.OldFile.Path))
		}
		if hasheadfile(info, delta.NewFile.Path) {
			fmt.Fprintf(fp, "b/<a href=\"../file/%s.html\">%s</a></b>\n", common.Pathunhide(delta.NewFile.Path), html.EscapeString(delta.NewFile.Path))
		} else {
			fmt.Fprintf(fp, "b/%s</b>\n", html.EscapeString(delta.NewFile.Path))
		}
		if delta.Flags&git.DiffFlagBinary > 0 {
			fmt.Fprintf(fp, "Binary files differ.\n")
			continue
		}

		err := ci.Diff.ForEach(func(file git.DiffDelta, progress float64) (git.DiffForEachHunkCallback, error) {
			j := 0
			return func(hunk git.DiffHunk) (git.DiffForEachLineCallback, error) {
				fmt.Fprintf(fp, "<a href=\"#h%d-%d\" id=\"h%d-%d\" class=\"h\">%s</a>\n", i, j, i, j, html.EscapeString(hunk.Header))

				j++
				k := 0
				return func(line git.DiffLine) error {
					if line.OldLineno == -1 {
						fmt.Fprintf(fp, "<a href=\"#h%d-%d-%d\" id=\"h%d-%d-%d\" class=\"i\">+", i, j, k, i, j, k)
					} else if line.NewLineno == -1 {
						fmt.Fprintf(fp, "<a href=\"#h%d-%d-%d\" id=\"h%d-%d-%d\" class=\"d\">-", i, j, k, i, j, k)
					} else {
						fmt.Fprintf(fp, " ")
					}
					fmt.Fprintf(fp, "%s\n", html.EscapeString(line.Content))
					if line.OldLineno == -1 || line.NewLineno == -1 {
						fmt.Fprintf(fp, "</a>")
					}

					k++
					return nil
				}, nil
			}, nil
		}, git.DiffDetailLines)
		if err != nil {
			return err
		}
	}

	fmt.Fprintf(fp, "</pre>\n")
	writefooter(fp)
	return nil
}
