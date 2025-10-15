package common

import (
	"bytes"
	"fmt"
	"maps"
	"os"
	"slices"
	"strings"
	"sync"
)

const barWidth = 50

type progressBar struct {
	what        string
	done, total int
	percent     float64 // 0..1
}

var Printer = ProgressPrinter{
	progress: make(map[string]progressBar),
}

type ProgressPrinter struct {
	mu           sync.Mutex
	progress     map[string]progressBar
	printedLines int // how many lines were printed last render
}

func (pp *ProgressPrinter) Progress(what string, done, total int) {
	pp.mu.Lock()
	defer pp.mu.Unlock()

	bar := pp.progress[what]
	// Keep previous total if caller passes -1 or <0
	if total < 0 {
		total = bar.total
	}

	percent := float64(done) / float64(total)
	if percent < 0 {
		percent = 0
	} else if percent > 1 {
		percent = 1
	}

	pp.progress[what] = progressBar{
		what:    what,
		done:    done,
		total:   total,
		percent: percent,
	}
	pp.print()
}

func (pp *ProgressPrinter) print() {
	if Verbose || Quiet {
		return
	}

	// Snapshot
	bars := slices.Collect(maps.Values(pp.progress))
	slices.SortFunc(bars, func(a, b progressBar) int {
		switch {
		case a.percent > b.percent:
			return -1
		case a.percent < b.percent:
			return 1
		default:
			return strings.Compare(a.what, b.what)
		}
	})

	var out bytes.Buffer

	// Move cursor up to the first progress line (if any previously printed)
	if pp.printedLines > 0 {
		fmt.Fprintf(&out, "\x1b[%dA", pp.printedLines)
	}

	// Render current bars
	for _, bar := range bars {
		out.WriteString("\r\x1b[2K")
		out.WriteString(formatLine(bar))
		out.WriteByte('\n')
	}

	// If fewer lines than before, clear leftovers
	if len(bars) < pp.printedLines {
		diff := pp.printedLines - len(bars)
		for i := 0; i < diff; i++ {
			out.WriteString("\r\x1b[2K\n")
		}
		// Move cursor back up over the cleared blank lines,
		// so the cursor ends at the last live line (if any)
		if len(bars) > 0 {
			fmt.Fprintf(&out, "\x1b[%dA", diff)
		}
	}
	pp.printedLines = len(bars)

	os.Stdout.Write(out.Bytes())
}

func formatLine(bar progressBar) string {
	var b bytes.Buffer
	// Left label padded for alignment
	fmt.Fprintf(&b, "%-40s [", bar.what)

	if bar.total == 0 {
		// Match your old "0 commits" behavior
		for i := 0; i < barWidth; i++ {
			b.WriteByte('#')
		}
		b.WriteString("] 100.0% (0 / 0)")
		return b.String()
	}

	pos := int(float64(barWidth) * bar.percent)
	if pos < 0 {
		pos = 0
	} else if pos > barWidth {
		pos = barWidth
	}
	for i := range barWidth {
		if i < pos {
			b.WriteByte('#')
		} else {
			b.WriteByte('-')
		}
	}
	fmt.Fprintf(&b, "] %5.1f%% (%d / %d)", bar.percent*100.0, bar.done, bar.total)
	return b.String()
}

func (pp *ProgressPrinter) Done(what string) {
	pp.mu.Lock()
	defer pp.mu.Unlock()

	delete(pp.progress, what)
	pp.print()
}
