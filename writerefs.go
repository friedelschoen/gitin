package gitin

import (
	"fmt"
	"html"
	"io"
	"strings"

	git "github.com/jeffwelling/git2go/v37"
)

const MAXSUMMARY = 30

func writerefheader(fp io.Writer, title string) {
	fmt.Fprintf(fp,
		"<div class=\"ref\">\n<h2>%s</h2>\n<table>"+
			"<thead>\n<tr><td class=\"expand\">Name</td>"+
			"<td>Last commit date</td>"+
			"<td>Author</td>\n</tr>\n"+
			"</thead><tbody>\n",
		title)
}

func writereffooter(fp io.Writer) {
	fmt.Fprintf(fp, "</tbody></table></div>\n")
}

func writerefs(fp io.Writer, info *repoinfo, relpath int, current *git.Reference) int {

	isbranch := true

	writerefheader(fp, "Branches")

	for _, refinfo := range info.refs {
		iscurrent := current.Cmp(refinfo.ref) == 0

		if isbranch && refinfo.istag {
			writereffooter(fp)
			writerefheader(fp, "Tags")
			isbranch = false
		}

		author := refinfo.commit.Author()
		summary := refinfo.commit.Summary()
		oid := refinfo.commit.Id().String()

		if len(summary) > MAXSUMMARY {
			summary = summary[:MAXSUMMARY] + "..."
		}

		escapename := pathunhide(refinfo.refname)

		if iscurrent {
			fmt.Fprintf(fp, "<tr class=\"current-ref\"><td>")
		} else {
			fmt.Fprintf(fp, "<tr><td>")
		}
		/* is current */
		fmt.Fprintf(fp, "<a href=\"%s%s/\">%s</a>", strings.Repeat("../", relpath), escapename, html.EscapeString(refinfo.refname))
		fmt.Fprintf(fp, " <small>at \"<a href=\"%scommit/%s.html\">%s</a>\"</small>", strings.Repeat("../", relpath), oid, summary)

		fmt.Fprintf(fp, "</td><td>")
		if author != nil {
			fmt.Fprintf(fp, "%s", rfc3339(author.When))
		}
		fmt.Fprintf(fp, "</td><td>")
		if author != nil {
			fmt.Fprintf(fp, "%s", html.EscapeString(author.Name))
		}
		fmt.Fprintf(fp, "</td></tr>\n")
	}

	writereffooter(fp)

	return 0
}
