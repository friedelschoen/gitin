package gitin

import (
	"fmt"
	"html"
	"io"
	"slices"
	"time"

	git "github.com/jeffwelling/git2go/v37"
)

const SECONDSINDAY = 60 * 60 * 24
const DAYSINYEAR = 365

type authorcount struct {
	name  string
	email string
	count int
}

type datecount struct {
	day   int64
	count int
	refs  string
}

type loginfo struct {
	authorcount []authorcount

	datecount []datecount
	bymonth   bool
}

var months = []string{
	"January", "February", "March", "April", "May", "Juni",
	"July", "August", "September", "October", "November", "December",
}

func incrementauthor(authorcount []authorcount, author *git.Signature) bool {
	for i, ac := range authorcount {
		if author.Name == ac.name && author.Email == authorcount[i].email {
			authorcount[i].count++
			return true
		}
	}
	return false
}

func writediagram(file io.Writer, li *loginfo) {
	/* Constants for the SVG size and scaling */
	var width int = 1200
	var height int = 500

	var padding_bottom = 100
	var padding_top = 100
	var padding_left = 20
	var padding_right = 20

	var point_radius = 3
	var char_width = 5

	var color string = "#3498db"

	var max_commits = 0
	for _, dc := range li.datecount {
		if max_commits < dc.count {
			max_commits = dc.count
		}
	}

	/* SVG header */
	fmt.Fprintf(file, "<svg id=\"shortlog-graph\" xmlns=\"http://www.w3.org/2000/svg\" viewbox=\"0 0 %d %d\" width=\"100%%\">\n", width, height)

	/* Scaling factors for graph */
	var x_scale = float64(width-padding_left-padding_right) / float64(len(li.datecount)-1)
	var y_scale = float64(height-padding_top-padding_bottom) / float64(max_commits)

	/* Draw the line graph from right to left */
	for i, dc := range li.datecount {
		x1 := width - padding_right - int(float64(i)*x_scale)
		y1 := height - padding_bottom - int(float64(dc.count)*y_scale)
		x2 := width - padding_right - int(float64(i+1)*x_scale)
		y2 := height - padding_bottom - int(float64(dc.count)*y_scale)

		fmt.Fprintf(
			file,
			"  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"%s\" stroke-width=\"2\"/>\n",
			x1, y1, x2, y2, color)
	}

	/* Add vertical labels for months, also right to left */
	for i, dc := range li.datecount {
		var x int = width - padding_right - int(float64(i)*x_scale) /* Reversed X */

		if dc.count > 0 {
			var y int = height - padding_bottom + 10 /* Adjust for spacing below the graph */
			var ty int = height - padding_bottom - int(float64(dc.count)*y_scale)

			tm := time.Unix(li.datecount[i].day*SECONDSINDAY, 0)

			fmt.Fprintf(
				file,
				"  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">",
				x-2, y, x-2, y)

			if li.bymonth {
				fmt.Fprintf(file, "%s %d", tm.Month(), tm.Year()+1900)
			} else {
				fmt.Fprintf(file, "%d %s %d", tm.Day(), tm.Month(), tm.Year()+1900)
			}
			fmt.Fprintf(file, "</text>\n")

			fmt.Fprintf(
				file,
				"  <text x=\"%d\" y=\"%d\" font-size=\"10px\" text-anchor=\"middle\">%d</text>\n",
				x, ty-10, li.datecount[i].count)
			fmt.Fprintf(file, "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"%s\"/>\n", x, ty,
				point_radius, color)
		}
		if li.datecount[i].refs != "" {
			var y2 int = height - int(float64(dc.count)*y_scale) - padding_bottom
			fmt.Fprintf(
				file,
				"  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#000\" stroke-width=\"1\" stroke-dasharray=\"4\"/>\n",
				x, 20+char_width*len(dc.refs), x, y2-30)

			fmt.Fprintf(
				file,
				"  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">%s</text>\n",
				x-2, 10, x-2, 10, dc.refs)
		}
	}

	/* SVG footer */
	fmt.Fprintf(file, "</svg>\n")
}

func mergedatecount(li *loginfo) {
	/* Initialize the first month with the first day */

	if len(li.datecount) == 0 {
		return
	}

	li.bymonth = true

	first_day := time.Unix(li.datecount[0].day*SECONDSINDAY, 0)

	/* Iterate through the rest of datecount */
	writeptr := 0
	for readptr := 1; readptr < len(li.datecount); readptr++ {
		current_day := time.Unix(li.datecount[readptr].day*SECONDSINDAY, 0)

		if current_day.Month() == first_day.Month() && current_day.Year() == first_day.Year() {
			/* Same month, accumulate counts */
			li.datecount[writeptr].count += li.datecount[readptr].count

			/* Append references if present */
			if li.datecount[readptr].refs != "" {
				if li.datecount[writeptr].refs != "" {
					li.datecount[writeptr].refs += ", "
				}
				li.datecount[writeptr].refs += li.datecount[readptr].refs
			}
		} else {
			/* Move to the next month */
			writeptr++

			/* Copy the new month entry to the next position */
			li.datecount[writeptr] = li.datecount[readptr]

			/* Update first_day for the new month */
			first_day = current_day
		}
	}

	/* Return the number of merged entries */
	li.datecount = li.datecount[:writeptr+1]
}

func countlog(info *repoinfo, head *git.Commit, li *loginfo) error {

	w, err := info.repo.Walk()
	if err != nil {
		return err
	}

	if err := w.Push(head.Id()); err != nil {
		return err
	}

	var previous int64 = 0
	_ = w.Iterate(func(commit *git.Commit) bool {
		author := commit.Author()
		days := author.When.Unix() / SECONDSINDAY

		if !incrementauthor(li.authorcount, author) {
			/* set new author */
			li.authorcount = append(li.authorcount, authorcount{
				count: 1,
				name:  author.Name,
				email: author.Email,
			})
		}

		if len(li.datecount) == 0 {
			previous = days + 1
		}
		for previous > days {
			previous--

			/* init new date */
			li.datecount = append(li.datecount, datecount{
				count: 0,
				day:   previous,
			})
		}

		if previous == days {
			li.datecount[len(li.datecount)-1].count++
		}
		return true
	})

	slices.SortFunc(li.authorcount, func(left, right authorcount) int {
		return right.count - left.count
	})
	return nil
}

func addrefcount(info *repoinfo, li *loginfo) error {

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
		ref = newref

		commitobj, err := ref.Peel(git.ObjectCommit)
		if err != nil {
			continue
		}
		commit, _ := commitobj.AsCommit()

		days := commit.Author().When.Unix() / SECONDSINDAY

		for i, dc := range li.datecount {
			if dc.day == days {
				if dc.refs != "" {
					li.datecount[i].refs += ", "
				}
				if ref.IsTag() {
					li.datecount[i].refs += "["
				}
				li.datecount[i].refs += ref.Shorthand()
				if ref.IsTag() {
					li.datecount[i].refs += "]"
				}
				break
			}
		}
	}
	return nil
}

func writeauthorlog(fp io.Writer, li *loginfo) {
	fmt.Fprintf(fp, "<table><thead>\n<tr><td class=\"num\">Count</td>"+
		"<td class=\"expand\">Author</td>"+
		"<td>E-Mail</td></tr>\n</thead><tbody>\n")

	for _, ac := range li.authorcount {
		fmt.Fprintf(fp, "<tr><td>%d</td><td>%s</td><td>%s</td></tr>", ac.count,
			html.EscapeString(ac.name), html.EscapeString(ac.email))
	}

	fmt.Fprintf(fp, "</tbody></table>")
}

func writeshortlog(fp io.Writer, info *repoinfo, head *git.Commit) error {
	var li loginfo
	if err := countlog(info, head, &li); err != nil {
		return err
	}
	if err := addrefcount(info, &li); err != nil {
		return err
	}
	if len(li.authorcount) == 0 {
		return nil
	}
	if len(li.datecount) > DAYSINYEAR {
		mergedatecount(&li)
	}
	fmt.Fprintf(fp, "<h2>Shortlog</h2>")
	writeauthorlog(fp, &li)

	fmt.Fprintf(fp, "<h2>Commit Graph</h2>")
	writediagram(fp, &li)
	return nil
}
