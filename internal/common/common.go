package common

import (
	"bytes"
	"fmt"
	"os"
	"strings"
)

var Force bool
var Verbose bool
var Columnate bool
var Quiet bool

const bar_width = 80 /* The width of the progress bar */

func Printprogress[T Integer](indx T, ncommits T, what string) {
	if Verbose || Quiet {
		return
	}

	var line bytes.Buffer
	fmt.Fprintf(&line, "\r%s [", what)

	/* Handle zero commits case */
	if ncommits == 0 {
		line.WriteString("##################################################] 100.0%% (0 / 0)")
	} else {
		progress := float64(indx) / float64(ncommits)

		/* Print the progress bar */
		var pos int = int(bar_width * progress)
		for i := range bar_width {
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

func Splitunit(size int64) (int64, string) {
	units := []string{"B", "KiB", "MiB", "GiB", "TiB"}
	for i, unit := range units {
		if size < 1024 || i == len(units)-1 {
			return size, unit
		}
		size /= 1024
	}
	return size, "<invalid>"
}

func Escaperefname(refname string) string {
	return strings.ReplaceAll(refname, "/", "-")
}

func Pathunhide(path string) string {
	runes := []rune(path)
	for i, chr := range runes {
		if chr == '.' && (i == 0 || runes[i-1] == '/' && runes[i+1] != '/') {
			runes[i] = '-'
		}
	}
	return string(runes)
}
