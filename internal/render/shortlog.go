package render

import (
	"fmt"
	"html"
	"io"
	"slices"
	"strings"
	"time"

	"github.com/friedelschoen/gitin-go/internal/wrapper"
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
	refs  []string
}

type loginfo struct {
	authorcount []authorcount

	datecount []datecount
	bymonth   bool
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

type padding struct {
	bottom, top, left, right int
}

func writediagram(file io.Writer, li *loginfo) {
	/* Constants for the SVG size and scaling */
	const width = 1200
	const height = 500

	padding := padding{bottom: 100, top: 100, left: 20, right: 20}

	pointRadius := 3
	charWidth := 4

	color := "#3498db"

	maxCommits := 0
	for _, dc := range li.datecount {
		maxCommits = max(maxCommits, dc.count)
	}

	/* SVG header */
	fmt.Fprintf(file, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\">\n", width, height)

	/* Scaling factors for graph */
	xScale := float64(width-padding.left-padding.right) / float64(len(li.datecount)-1)
	yScale := float64(height-padding.top-padding.bottom) / float64(maxCommits)

	/* Draw the line graph from right to left */
	for i := 0; i < len(li.datecount)-1; i++ {
		x1 := width - padding.right - int(float64(i)*xScale)
		y1 := height - padding.bottom - int(float64(li.datecount[i].count)*yScale)
		x2 := width - padding.right - int(float64(i+1)*xScale)
		y2 := height - padding.bottom - int(float64(li.datecount[i+1].count)*yScale)

		fmt.Fprintf(
			file,
			"  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"%s\" stroke-width=\"2\" />\n",
			x1, y1, x2, y2, color)
	}

	/* Add vertical labels for months, also right to left */
	for i, dc := range li.datecount {
		x := width - padding.right - int(float64(i)*xScale) /* Reversed X */

		if dc.count > 0 || len(dc.refs) > 0 {
			y := height - padding.bottom + 10 /* Adjust for spacing below the graph */
			ty := height - padding.bottom - int(float64(dc.count)*yScale)

			tm := time.Unix(dc.day*SECONDSINDAY, 0)

			fmt.Fprintf(
				file,
				"  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">",
				x-2, y, x-2, y)

			if li.bymonth {
				fmt.Fprintf(file, "%s %d", tm.Month(), tm.Year())
			} else {
				fmt.Fprintf(file, "%d %s %d", tm.Day(), tm.Month(), tm.Year())
			}
			fmt.Fprintf(file, "</text>\n")

			fmt.Fprintf(
				file,
				"  <text x=\"%d\" y=\"%d\" font-size=\"10px\" text-anchor=\"middle\">%d</text>\n",
				x, ty-10, dc.count)
			fmt.Fprintf(file, "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"%s\"/>\n", x, ty,
				pointRadius, color)
		}
		if len(dc.refs) > 0 {
			str := strings.Join(dc.refs, ", ")
			y2 := height - int(float64(dc.count)*yScale) - padding.bottom
			fmt.Fprintf(
				file,
				"  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#000\" stroke-width=\"1\" stroke-dasharray=\"4\"/>\n",
				x, 20+charWidth*len(str), x, y2-30)

			fmt.Fprintf(
				file,
				"  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">%s</text>\n",
				x-2, 10, x-2, 10, str)
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

	firstday := time.Unix(li.datecount[0].day*SECONDSINDAY, 0)

	/* Iterate through the rest of datecount */
	writeptr := 0
	for readptr := 1; readptr < len(li.datecount); readptr++ {
		currentday := time.Unix(li.datecount[readptr].day*SECONDSINDAY, 0)

		if currentday.Month() == firstday.Month() && currentday.Year() == firstday.Year() {
			/* Same month, accumulate counts */
			li.datecount[writeptr].count += li.datecount[readptr].count

			/* Append references if present */
			if len(li.datecount[readptr].refs) > 0 {
				li.datecount[writeptr].refs = append(li.datecount[writeptr].refs, li.datecount[readptr].refs...)
			}
		} else {
			/* Move to the next month */
			writeptr++

			/* Copy the new month entry to the next position */
			li.datecount[writeptr] = li.datecount[readptr]

			/* Update first_day for the new month */
			firstday = currentday
		}
	}

	/* Return the number of merged entries */
	li.datecount = li.datecount[:writeptr+1]
}

func countlog(info *wrapper.RepoInfo, head *git.Commit, li *loginfo) error {
	w, err := info.Repo.Walk()
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

func addrefcount(info *wrapper.RepoInfo, li *loginfo) error {
	iter, err := info.Repo.NewReferenceIterator()
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
				var str string
				if ref.IsTag() {
					str = fmt.Sprintf("[%s]", ref.Shorthand())
				} else {
					str = ref.Shorthand()
				}
				li.datecount[i].refs = append(li.datecount[i].refs, str)
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

func writeshortlog(fp io.Writer, svgfp io.Writer, info *wrapper.RepoInfo, head *git.Commit) error {
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
	fmt.Fprintf(fp, "<img id=\"shortlog-graph\" src=\"log.svg\" />\n")

	writediagram(svgfp, &li)
	return nil
}
