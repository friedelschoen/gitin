package gitin

import (
	"fmt"
	"html"
	"io"
	"slices"
	"strings"

	git "github.com/jeffwelling/git2go/v37"
)

// #include "fmt.Fprintf.h"
// #include "writer.h"

// #include <assert.h>
// #include <ctype.h>
// #include <git2/commit.h>
// #include <string.h>

const MAXLINESTR = 80

func hasheadfile(info *repoinfo, filename string) bool { // static int hasheadfile(const struct repoinfo* info, const char* filename) {
	return slices.Contains(info.headfiles, filename)
}

func writecommit(fp io.Writer, info *repoinfo, commit *git.Commit, ci *commitinfo, parentlink bool) error { // void writecommit(io.Writer fp, const struct repoinfo* info, git.Commit* commit,
	// // const struct commitinfo* ci, int parentlink) {
	// var delta *git.Diff_delta // const git.Diff_delta* delta;
	// var hunk *git.Diff_hunk   // const git.Diff_hunk*  hunk;
	// var line *git.Diff_line   // const git.Diff_line*  line;
	// var nhunks uint64
	// var nhunklines uint64
	// var i uint64
	// var j uint64
	// var k uint64         // size_t                nhunks, nhunklines, i, j, k;
	// var linestr [80]byte // char                  linestr[80];
	// var c int            // int                   c;

	author := commit.Author()
	summary := commit.Summary()
	msg := commit.Message()
	oid := commit.Id().String()
	var parentoid string
	if parentcommit := commit.ParentId(0); parentcommit != nil {
		parentoid = parentcommit.String()
	}

	writeheader(fp, info, 1, false, info.name, fmt.Sprintf("%s (%s)", html.EscapeString(summary), oid)) // writeheader(fp, info, 1, 0, info->name, "%y (%s)", summary, oid);
	fmt.Fprintf(fp, "<pre>")                                                                            // fmt.Fprintf("<pre>", fp);

	fmt.Fprintf(fp, "<b>commit</b> <a href=\"%s.html\">%s</a>\n", oid, oid) // fmt.Fprintf(fp, "<b>commit</b> <a href=\"%s.html\">%s</a>\n", oid, oid);

	if parentoid != "" { // if (*parentoid) {
		if parentlink { // if (parentlink)
			fmt.Fprintf(fp, "<b>parent</b> <a href=\"%s.html\">%s</a>\n", parentoid, parentoid) // fmt.Fprintf(fp, "<b>parent</b> <a href=\"%s.html\">%s</a>\n", parentoid, parentoid);
		} else {
			fmt.Fprintf(fp, "<b>parent</b> %s\n", parentoid) // fmt.Fprintf(fp, "<b>parent</b> %s\n", parentoid);
		}
	}
	if author != nil { // if (author)
		fmt.Fprintf(fp, "<b>Author:</b> %s &lt;<a href=\"mailto:%s\">%s</a>&gt;\n<b>Date:</b>   %T\n",
			html.EscapeString(author.Name), html.EscapeString(author.Email), html.EscapeString(author.Email), rfc3339(author.When.UTC())) // author->name, author->email, author->email, author->when);
	}
	if msg != "" { // if (msg)
		fmt.Fprintf(fp, "\n%s\n", html.EscapeString(msg)) // fmt.Fprintf(fp, "\n%y\n", msg);
	}
	if len(ci.Deltas) == 0 { // if (!ci->deltas)
		return nil // return;
	}
	if len(ci.Deltas) > 1000 || ci.Addcount > 100000 || ci.Delcount > 100000 { // if (ci->ndeltas > 1000 || ci->addcount > 100000 || ci->delcount > 100000) {
		fmt.Fprintf(fp, "Diff is too large, output suppressed.\n") // fmt.Fprintf("Diff is too large, output suppressed.\n", fp);
		return nil                                                 // return;
	}

	/* diff stat */
	fmt.Fprintf(fp, "<b>Diffstat:</b>\n<table>") // fmt.Fprintf("<b>Diffstat:</b>\n<table>", fp);
	for i, del := range ci.Deltas {              // for (i = 0; i < ci->ndeltas; i++) {
		delta := del.delta // delta = git.Patch_get_delta(ci->deltas[i].patch);

		var c rune
		switch delta.Status { // switch (delta->status) {
		case git.DeltaAdded:
			c = 'A' // c = 'A';
		case git.DeltaCopied:
			c = 'C' // c = 'C';
		case git.DeltaDeleted:
			c = 'D' // c = 'D';
		case git.DeltaModified:
			c = 'M' // c = 'M';
		case git.DeltaRenamed:
			c = 'R' // c = 'R';
		case git.DeltaTypeChange:
			c = 'T' // c = 'T';
		}
		if c == 0 { // if (c == ' ')
			fmt.Fprintf(fp, "<tr><td> ") // fmt.Fprintf(fp, "<tr><td>%c", c);
		} else {
			fmt.Fprintf(fp, "<tr><td class=\"%c\">%c", c, c) // fmt.Fprintf(fp, "<tr><td class=\"%c\">%c", c, c);
		}
		fmt.Fprintf(fp, "</td><td><a href=\"#h%d\">%s", i, html.EscapeString(delta.OldFile.Path)) // fmt.Fprintf(fp, "</td><td><a href=\"#h%zu\">%y", i, delta->old_file.path);
		if delta.OldFile.Path != delta.NewFile.Path {                                             // if (strcmp(delta->old_file.path, delta->new_file.path))
			fmt.Fprintf(fp, " -&gt; \n%s", html.EscapeString(delta.NewFile.Path)) // fmt.Fprintf(fp, " -&gt; \n%y", delta->new_file.path);
		}
		add := del.Addcount      // add     = ci->deltas[i].addcount;
		deletion := del.Delcount // del     = ci->deltas[i].delcount;

		// var total = unsafe.Sizeof(linestr{}) - 2 // total   = sizeof(linestr) - 2;
		changed := add + deletion // changed = add + del;
		if changed > MAXLINESTR { // if (changed > total) {
			if add > 0 { // if (add)
				add = int(float64(MAXLINESTR*add)/float64(changed)) + 1 // add = ((double) total / changed * add) + 1;
			}
			if deletion > 0 { // if (del)
				deletion = int(float64(MAXLINESTR*deletion)/float64(changed)) + 1 // del = ((double) total / changed * del) + 1;
			}
		}

		fmt.Fprintf(fp, "</a></td><td class=\"num\">%d</td><td class=\"expand\"><span class=\"i\">%s</span><span class=\"d\">%s</span></td></tr>\n",
			del.Addcount+del.Delcount, strings.Repeat("+", add), strings.Repeat("-", deletion)) // ci->deltas[i].addcount + ci->deltas[i].delcount);
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
		len(ci.Deltas), plFile, ci.Addcount, plAdd, ci.Delcount, plDel) // ci->delcount, ci->delcount == 1 ? "" : "s");

	fmt.Fprintf(fp, "<hr/>") // fmt.Fprintf("<hr/>", fp);

	for i, del := range ci.Deltas { // for (i = 0; i < ci->ndeltas; i++) {
		delta := del.delta // delta            = git.Patch_get_delta(patch);

		if hasheadfile(info, delta.OldFile.Path) { // if (hasheadfile(info, delta->old_file.path))
			fmt.Fprintf(fp, "<b>diff --git a/<a id=\"h%d\" href=\"../file/%s.html\">%s</a> ", i,
				pathunhide(delta.OldFile.Path), html.EscapeString(delta.OldFile.Path)) // delta->old_file.path, delta->old_file.path);
		} else {
			fmt.Fprintf(fp, "<b>diff --git a/%s ", html.EscapeString(delta.OldFile.Path)) // fmt.Fprintf(fp, "<b>diff --git a/%y ", delta->old_file.path);
		}
		if hasheadfile(info, delta.NewFile.Path) { // if (hasheadfile(info, delta->new_file.path))
			fmt.Fprintf(fp, "b/<a href=\"../file/%s.html\">%s</a></b>\n", pathunhide(delta.NewFile.Path), html.EscapeString(delta.NewFile.Path))
		} else {
			fmt.Fprintf(fp, "b/%s</b>\n", html.EscapeString(delta.NewFile.Path)) // fmt.Fprintf(fp, "b/%y</b>\n", delta->new_file.path);
		}
		if delta.Flags&git.DiffFlagBinary > 0 { // if (delta->flags & git.DIFF_FLAG_BINARY) {
			fmt.Fprintf(fp, "Binary files differ.\n") // fmt.Fprintf("Binary files differ.\n", fp);
			continue                                  // continue;
		}

		err := ci.diff.ForEach(func(file git.DiffDelta, progress float64) (git.DiffForEachHunkCallback, error) {
			j := 0
			return func(hunk git.DiffHunk) (git.DiffForEachLineCallback, error) {
				fmt.Fprintf(fp, "<a href=\"#h%d-%d\" id=\"h%d-%d\" class=\"h\">%s</a>\n", i, j, i, j, html.EscapeString(hunk.Header)) // hunk->header);

				j++
				k := 0
				return func(line git.DiffLine) error {
					if line.OldLineno == -1 { // if (line->old_lineno == -1)
						fmt.Fprintf(fp, "<a href=\"#h%d-%d-%d\" id=\"h%d-%d-%d\" class=\"i\">+", i, j, k, i, j, k) // k, i, j, k);
					} else if line.NewLineno == -1 { // else if (line->new_lineno == -1)
						fmt.Fprintf(fp, "<a href=\"#h%d-%d-%d\" id=\"h%d-%d-%d\" class=\"d\">-", i, j, k, i, j, k) // k, i, j, k);
					} else {
						fmt.Fprintf(fp, " ")
					}
					fmt.Fprintf(fp, "%s\n", html.EscapeString(line.Content))
					if line.OldLineno == -1 || line.NewLineno == -1 { // if (line->old_lineno == -1 || line->new_lineno == -1)
						fmt.Fprintf(fp, "</a>") // fmt.Fprintf("</a>", fp);
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

	fmt.Fprintf(fp, "</pre>\n") // fmt.Fprintf("</pre>\n", fp);
	writefooter(fp)             // writefooter(fp);
	return nil
}
