package gitin

import (
	"fmt"
	"html"
	"io"
	"slices"
	"time"

	git "github.com/jeffwelling/git2go/v37"
)

// #include "fmt.Fprintf.h"
// #include "writer.h"

// #include <git2/commit.h>
// #include <git2/revwalk.h>
// #include <string.h>

// #define MAXREFS      64
const SECONDSINDAY = 60 * 60 * 24
const DAYSINYEAR = 365

type authorcount struct {
	name  string // char* name;
	email string // char* email;
	count int    // int   count;
} // };

type datecount struct {
	day   int64  // size_t day;
	count int    // int    count;
	refs  string // char   refs[MAXREFS];
} // };

type loginfo struct {
	authorcount []authorcount // struct authorcount* authorcount;

	datecount []datecount // struct datecount* datecount;
	bymonth   bool        // int               bymonth;
} // };

var months = []string{ // static const char* months[] = {
	"January", "February", "March", "April", "May", "Juni",
	"July", "August", "September", "October", "November", "December",
} // };

func incrementauthor(authorcount []authorcount, author *git.Signature) bool { // const git.Signature* author) {
	for i, ac := range authorcount { // for (int i = 0; i < nauthorcount; i++) {
		if author.Name == ac.name && author.Email == authorcount[i].email { // !strcmp(author->email, authorcount[i].email)) {
			authorcount[i].count++ // authorcount[i].count++;
			return true
		}
	}
	return false
}

func writediagram(file io.Writer, li *loginfo) { // static void writediagram(io.Writer file, struct loginfo* li) {
	/* Constants for the SVG size and scaling */
	var width int = 1200 // const int width  = 1200;
	var height int = 500 // const int height = 500;

	var padding_bottom = 100 // const int padding_bottom = 100;
	var padding_top = 100    // const int padding_top    = 100;
	var padding_left = 20    // const int padding_left   = 20;
	var padding_right = 20   // const int padding_right  = 20;

	var point_radius = 3 // const int point_radius = 3;
	var char_width = 5   // const int char_width   = 5;

	var color string = "#3498db" // const char* color = "#3498db";

	var max_commits = 0               // int max_commits = 0;
	for _, dc := range li.datecount { // for (int i = 0; i < li->ndatecount; i++) {
		if max_commits < dc.count { // if (max_commits < li->datecount[i].count) {
			max_commits = dc.count // max_commits = li->datecount[i].count;
		}
	}

	/* SVG header */
	fmt.Fprintf(
		file,
		"<svg id=\"shortlog-graph\" xmlns=\"http://www.w3.org/2000/svg\" viewbox=\"0 0 %d %d\" width=\"100%%\">\n",
		width, height) // width, height);

	/* Scaling factors for graph */
	var x_scale = float64(width-padding_left-padding_right) / float64(len(li.datecount)-1) // double x_scale = (double) (width - padding_left - padding_right) / (li->ndatecount - 1);
	var y_scale = float64(height-padding_top-padding_bottom) / float64(max_commits)        // double y_scale = (double) (height - padding_top - padding_bottom) / max_commits;

	/* Draw the line graph from right to left */
	for i, dc := range li.datecount { // for (int i = 0; i < li->ndatecount - 1; i++) {
		var x1 int = width - padding_right - int(float64(i)*x_scale)          // int x1 = width - padding_right - i * x_scale;
		var y1 int = height - padding_bottom - int(float64(dc.count)*y_scale) // int y1 = height - padding_bottom - li->datecount[i].count * y_scale;
		var x2 int = width - padding_right - int(float64(i+1)*x_scale)        // int x2 = width - padding_right - (i + 1) * x_scale;
		var y2 int = height - padding_bottom - int(float64(dc.count)*y_scale) // int y2 = height - padding_bottom - li->datecount[i + 1].count * y_scale;

		fmt.Fprintf(
			file,
			"  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"%s\" stroke-width=\"2\"/>\n",
			x1, y1, x2, y2, color) // x1, y1, x2, y2, color);
	}

	/* Add vertical labels for months, also right to left */
	for i, dc := range li.datecount { // for (int i = 0; i < li->ndatecount; i++) {
		var x int = width - padding_right - int(float64(i)*x_scale) /* Reversed X */ // int x = width - padding_right - i * x_scale; /* Reversed X */

		if dc.count > 0 { // if (li->datecount[i].count > 0) {
			var y int = height - padding_bottom + 10                              /* Adjust for spacing below the graph */ // int y  = height - padding_bottom + 10; /* Adjust for spacing below the graph */
			var ty int = height - padding_bottom - int(float64(dc.count)*y_scale) // int ty = height - padding_bottom - li->datecount[i].count * y_scale;

			tm := time.Unix(li.datecount[i].day*SECONDSINDAY, 0)

			fmt.Fprintf(
				file,
				"  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">",
				x-2, y, x-2, y) // x - 2, y, x - 2, y);

			if li.bymonth { // if (li->bymonth)
				fmt.Fprintf(file, "%s %d", tm.Month(), tm.Year()+1900) // fmt.Fprintf(file, "%s %d", months[time.tm_mon], time.tm_year + 1900);
			} else {
				fmt.Fprintf(file, "%d %s %d", tm.Day(), tm.Month(), tm.Year()+1900) // fmt.Fprintf(file, "%d %s %d", time.tm_mday, months[time.tm_mon], time.tm_year + 1900);
			}
			fmt.Fprintf(file, "</text>\n") // fmt.Fprintf(file, "</text>\n");

			fmt.Fprintf(
				file,
				"  <text x=\"%d\" y=\"%d\" font-size=\"10px\" text-anchor=\"middle\">%d</text>\n",
				x, ty-10, li.datecount[i].count) // x, ty - 10, li->datecount[i].count);
			fmt.Fprintf(file, "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"%s\"/>\n", x, ty,
				point_radius, color) // point_radius, color);
		}
		if li.datecount[i].refs != "" { // if (li->datecount[i].refs[0]) {
			var y2 int = height - int(float64(dc.count)*y_scale) - padding_bottom // int y2 = height - li->datecount[i].count * y_scale - padding_bottom;
			fmt.Fprintf(
				file,
				"  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#000\" stroke-width=\"1\" stroke-dasharray=\"4\"/>\n",
				x, 20+char_width*len(dc.refs), x, y2-30) // x, 20 + char_width * strlen(li->datecount[i].refs), x, y2 - 30);

			fmt.Fprintf(
				file,
				"  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">%s</text>\n",
				x-2, 10, x-2, 10, dc.refs) // x - 2, 10, x - 2, 10, li->datecount[i].refs);
		}
	}

	/* SVG footer */
	fmt.Fprintf(file, "</svg>\n") // fmt.Fprintf(file, "</svg>\n");
}

func mergedatecount(li *loginfo) { // static void mergedatecount(struct loginfo* li) {
	/* Initialize the first month with the first day */
	// var writeptr int = 0 // int       writeptr = 0;
	// var first_day_secs time_t
	// var current_secs time_t // time_t    first_day_secs, current_secs;
	// var first_day tm
	// var current_day tm // struct tm first_day, current_day;

	if len(li.datecount) == 0 {
		return
	}

	li.bymonth = true // li->bymonth = 1;

	first_day := time.Unix(li.datecount[0].day*SECONDSINDAY, 0)

	/* Iterate through the rest of datecount */
	writeptr := 0
	for readptr := 1; readptr < len(li.datecount); readptr++ { // for (int readptr = 1; readptr < li->ndatecount; readptr++) {
		current_day := time.Unix(li.datecount[readptr].day*SECONDSINDAY, 0)

		if current_day.Month() == first_day.Month() && current_day.Year() == first_day.Year() { // if (current_day.tm_mon == first_day.tm_mon && current_day.tm_year == first_day.tm_year) {
			/* Same month, accumulate counts */
			li.datecount[writeptr].count += li.datecount[readptr].count // li->datecount[writeptr].count += li->datecount[readptr].count;

			/* Append references if present */
			if li.datecount[readptr].refs != "" {
				if li.datecount[writeptr].refs != "" {
					li.datecount[writeptr].refs += ", "
				}
				li.datecount[writeptr].refs += li.datecount[readptr].refs
			}
		} else {
			/* Move to the next month */
			writeptr++ // writeptr++;

			/* Copy the new month entry to the next position */
			li.datecount[writeptr] = li.datecount[readptr]

			/* Update first_day for the new month */
			first_day = current_day // first_day = current_day;
		}
	}

	/* Return the number of merged entries */
	li.datecount = li.datecount[:writeptr+1]
}

func countlog(info *repoinfo, head *git.Commit, li *loginfo) error { // static void countlog(const struct repoinfo* info, git.Commit* head, struct loginfo* li) {
	// var w *git.Revwalk = nil     // git.Revwalk*         w      = NULL;
	// var commit *git.Commit = nil // git.Commit*          commit = NULL;
	// var author *git.Signature    // const git.Signature* author;
	// var id git.Oid               // git.Oid              id;
	// var days uint64 = 0
	// var previous uint64 = 0 // size_t               days = 0, previous = 0;

	w, err := info.repo.Walk()
	if err != nil {
		return err
	}

	if err := w.Push(head.Id()); err != nil {
		return err
	}

	var previous int64 = 0
	_ = w.Iterate(func(commit *git.Commit) bool {
		author := commit.Author() // author = git.Commit_author(commit);
		days := author.When.Unix() / SECONDSINDAY

		if !incrementauthor(li.authorcount, author) { // if (!incrementauthor(li->authorcount, li->nauthorcount, author)) {
			/* set new author */
			li.authorcount = append(li.authorcount, authorcount{
				count: 1,
				name:  author.Name,
				email: author.Email,
			})
		}

		if len(li.datecount) == 0 { // if (li->ndatecount == 0)
			previous = days + 1 // previous = days + 1;
		}
		for previous > days { // while (previous > days) {
			previous-- // previous--;

			/* init new date */
			li.datecount = append(li.datecount, datecount{
				count: 0,
				day:   previous,
			})
		}

		if previous == days { // if (previous == days)
			li.datecount[len(li.datecount)-1].count++ // li->datecount[li->ndatecount - 1].count++;
		}
		return true
	})

	slices.SortFunc(li.authorcount, func(left, right authorcount) int {
		return right.count - left.count
	})
	return nil
}

func addrefcount(info *repoinfo, li *loginfo) error { // static void addrefcount(const struct repoinfo* info, struct loginfo* li) {
	// var iter *git.Reference_iterator // git.Reference_iterator* iter;
	// var commit *git.Commit           // git.Commit*             commit;
	// var ref *git.Reference
	// var newref *git.Reference // git.Reference *         ref, *newref;
	// var days uint64           // size_t                  days;

	iter, err := info.repo.NewReferenceIterator()
	if err != nil {
		return err
	}

	for {
		ref, err := iter.Next()
		if gerr, ok := err.(*git.GitError); ok && gerr.Code == git.ErrorCodeIterOver {
			break
		} else if err != nil {
			return err
		}
		if !ref.IsBranch() && !ref.IsTag() {
			continue
		}
		newref, err := ref.Resolve()
		if err != nil {
			continue
		}
		ref = newref // ref = newref;

		commitobj, err := ref.Peel(git.ObjectCommit)
		if err != nil {
			continue
		}
		commit, _ := commitobj.AsCommit()

		days := commit.Author().When.Unix() / SECONDSINDAY // days = git.Commit_time(commit) / SECONDSINDAY;

		for i, dc := range li.datecount { // for (int i = 0; i < li->ndatecount; i++) {
			if dc.day == days { // if (li->datecount[i].day == days) {
				if dc.refs != "" { // if (li->datecount[i].refs[0])
					li.datecount[i].refs += ", " // strlcat(li->datecount[i].refs, ", ", MAXREFS);
				}
				if ref.IsTag() { // if (git.Reference_is_tag(ref))
					li.datecount[i].refs += "[" // strlcat(li->datecount[i].refs, ", ", MAXREFS);
				}
				li.datecount[i].refs += ref.Shorthand() // strlcat(li->datecount[i].refs, ", ", MAXREFS);
				if ref.IsTag() {                        // if (git.Reference_is_tag(ref))
					li.datecount[i].refs += "]" // strlcat(li->datecount[i].refs, ", ", MAXREFS);
				}
				break // break;
			}
		}
	}
	return nil
}

func writeauthorlog(fp io.Writer, li *loginfo) { // static void writeauthorlog(io.Writer fp, const struct loginfo* li) {
	fmt.Fprintf(fp, "<table><thead>\n<tr><td class=\"num\">Count</td>"+
		"<td class=\"expand\">Author</td>"+
		"<td>E-Mail</td></tr>\n</thead><tbody>\n") // fp);

	for _, ac := range li.authorcount { // for (int i = 0; i < li->nauthorcount; i++) {
		fmt.Fprintf(fp, "<tr><td>%d</td><td>%s</td><td>%s</td></tr>", ac.count, // fmt.Fprintf(fp, "<tr><td>%d</td><td>%s</td><td>%s</td></tr>", li->authorcount[i].count,
			html.EscapeString(ac.name), html.EscapeString(ac.email)) // li->authorcount[i].name, li->authorcount[i].email);
	}

	fmt.Fprintf(fp, "</tbody></table>") // fmt.Fprintf("</tbody></table>", fp);
}

func writeshortlog(fp io.Writer, info *repoinfo, head *git.Commit) error { // void writeshortlog(io.Writer fp, const struct repoinfo* info, git.Commit* head) {
	var li loginfo                                    // struct loginfo li;
	if err := countlog(info, head, &li); err != nil { // countlog(info, head, &li);
		return err
	}
	if err := addrefcount(info, &li); err != nil { // addrefcount(info, &li);
		return err
	}
	if len(li.authorcount) == 0 { // if (!li.authorcount)
		return nil // return;
	}
	if len(li.datecount) > DAYSINYEAR { // if (li.ndatecount > DAYSINYEAR)
		mergedatecount(&li) // mergedatecount(&li);
	}
	fmt.Fprintf(fp, "<h2>Shortlog</h2>") // fmt.Fprintf("<h2>Shortlog</h2>", fp);
	writeauthorlog(fp, &li)              // writeauthorlog(fp, &li);

	fmt.Fprintf(fp, "<h2>Commit Graph</h2>") // fmt.Fprintf("<h2>Commit Graph</h2>", fp);
	writediagram(fp, &li)                    // writediagram(fp, &li);
	return nil
}
