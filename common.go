package gitin

import (
	"bytes"
	"fmt"
	"os"
	"strings"
)

var force bool
var verbose bool
var columnate bool
var quiet bool

func printprogress[T Integer](indx T, ncommits T, what string) {
	if verbose || quiet {
		return
	}

	var line bytes.Buffer
	line.WriteRune('\r')
	line.WriteString(what)
	line.WriteString(" [")

	/* Handle zero commits case */
	if ncommits == 0 {
		line.WriteString("##################################################] 100.0%% (0 / 0)")
	} else {
		progress := float64(indx) / float64(ncommits)
		bar_width := 50 /* The width of the progress bar */

		/* Print the progress bar */
		var pos int = int(float64(bar_width) * progress)
		for i := 0; i < bar_width; i++ {
			if i < pos {
				line.WriteRune('#')
			} else {
				line.WriteRune('-')
			}
		}

		/* Print the progress percentage and counts */
		fmt.Fprintf(&line, "] %5.1f%% (%d / %d)", progress*100, indx, ncommits)
	}
	/* Ensure the line is flushed and fully updated in the terminal */
	line.WriteTo(os.Stdout)
}

func splitunit(size int64) (int64, string) {
	unit := "B"
	if size > 1024 {
		unit = "KiB"
		size /= 1024
		if size > 1024 {
			unit = "MiB"
			size /= 1024
			if size > 1024 {
				unit = "GiB"
				size /= 1024
				if size > 1024 {
					unit = "TiB"
					size /= 1024
				}
			}
		}
	}
	return size, unit
}

func escaperefname(refname string) string {
	return strings.ReplaceAll(refname, "/", "-")
}
