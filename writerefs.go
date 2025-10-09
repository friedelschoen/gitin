package gitin

import (
	"fmt"
	"html"
	"io"
	"strings"

	git "github.com/jeffwelling/git2go/v37"
)

// #include "fmt.Fprintf.h"
// #include "writer.h"

// #include <ctype.h>
// #include <git2/commit.h>
// #include <limits.h>
// #include <string.h>

const MAXSUMMARY = 30

func writerefheader(fp io.Writer, title string) { // static void writerefheader(io.Writer fp, const char* title) {
	fmt.Fprintf(fp,
		"<div class=\"ref\">\n<h2>%s</h2>\n<table>"+
			"<thead>\n<tr><td class=\"expand\">Name</td>"+
			"<td>Last commit date</td>"+
			"<td>Author</td>\n</tr>\n"+
			"</thead><tbody>\n",
		title) // title);
}

func writereffooter(fp io.Writer) { // static void writereffooter(io.Writer fp) {
	fmt.Fprintf(fp, "</tbody></table></div>\n") // fmt.Fprintf("</tbody></table></div>\n", fp);
}

func writerefs(fp io.Writer, info *repoinfo, relpath int, current *git.Reference) int { // int writerefs(io.Writer fp, const struct repoinfo* info, int relpath, git.Reference* current) {
	// var escapename [NAME_MAX]byte
	// var oid [git.OID_SHA1_HEXSIZE + 1]byte
	// var summary [MAXSUMMARY + 2]byte // char escapename[NAME_MAX], oid[git.OID_SHA1_HEXSIZE + 1], summary[MAXSUMMARY + 2];
	isbranch := true // int  isbranch = 1;

	writerefheader(fp, "Branches") // writerefheader(fp, "Branches");

	for _, refinfo := range info.refs { // for (int i = 0; i < info->nrefs; i++) {
		iscurrent := current.Cmp(refinfo.ref) == 0 // int            iscurrent = !git.Reference_cmp(ref, current);

		if isbranch && refinfo.istag { // if (isbranch && info->refs[i].istag) {
			writereffooter(fp)         // writereffooter(fp);
			writerefheader(fp, "Tags") // writerefheader(fp, "Tags");
			isbranch = false           // isbranch = 0;
		}

		author := refinfo.commit.Author() // const git.Signature* author = git.Commit_author(commit);
		summary := refinfo.commit.Summary()
		oid := refinfo.commit.Id().String()

		if len(summary) > MAXSUMMARY { // if (strlen(summary) > MAXSUMMARY) {
			summary = summary[:MAXSUMMARY] + "..."
		}

		escapename := pathunhide(refinfo.refname)

		if iscurrent { // if (iscurrent)
			fmt.Fprintf(fp, "<tr class=\"current-ref\"><td>") // fmt.Fprintf(fp, "<tr class=\"current-ref\"><td>");
		} else {
			fmt.Fprintf(fp, "<tr><td>") // fmt.Fprintf(fp, "<tr><td>");
		}
		/* is current */
		fmt.Fprintf(fp, "<a href=\"%s%s/\">%s</a>", strings.Repeat("../", relpath), escapename, html.EscapeString(refinfo.refname)) // fmt.Fprintf(fp, "<a href=\"%r%s/\">%y</a>", relpath, name, name);
		fmt.Fprintf(fp, " <small>at \"<a href=\"%scommit/%s.html\">%s</a>\"</small>", strings.Repeat("../", relpath), oid, summary) // summary);

		fmt.Fprintf(fp, "</td><td>") // fmt.Fprintf("</td><td>", fp);
		if author != nil {           // if (author)
			fmt.Fprintf(fp, "%s", rfc3339(author.When)) // fmt.Fprintf(fp, "%t", author->when);
		}
		fmt.Fprintf(fp, "</td><td>") // fmt.Fprintf("</td><td>", fp);
		if author != nil {           // if (author)
			fmt.Fprintf(fp, "%s", html.EscapeString(author.Name)) // fmt.Fprintf(fp, "%y", author->name);
		}
		fmt.Fprintf(fp, "</td></tr>\n") // fmt.Fprintf("</td></tr>\n", fp);
	}

	writereffooter(fp) // writereffooter(fp);

	return 0 // return 0;
}
