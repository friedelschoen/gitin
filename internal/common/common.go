// Package common contains functionality used in other packages
package common

import (
	"strings"
)

var Force bool
var Verbose bool
var Columnate bool
var Quiet bool

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

func Relpath(n int) string {
	return strings.Repeat("../", n)
}
