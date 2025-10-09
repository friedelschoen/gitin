package gitin

import (
	"bufio"
	"io"
	"iter"
	"os"
	"slices"
	"strings"
)

func parsecache(buffer io.Reader) []indexinfo {
	scan := bufio.NewScanner(buffer)

	var indexes []indexinfo
	for scan.Scan() {
		fields := strings.SplitN(scan.Text(), ",", 3)
		indexes = append(indexes, indexinfo{
			repodir:     fields[0],
			name:        fields[1],
			description: fields[2],
		})
	}

	return indexes
}

func GetIndex(repos iter.Seq[string]) *gitininfo {
	var info gitininfo

	/* parse cache */
	if cachefp, err := os.Open(".cache/index"); err == nil {
		defer cachefp.Close()
		info.indexes = parsecache(cachefp)
	}

	/* fill cache with to update repos */
	for repodir := range repos {
		if idx := slices.IndexFunc(info.indexes, func(ii indexinfo) bool { return ii.repodir == repodir }); idx != -1 {
			info.indexes[idx].name = ""        /* to be filled */
			info.indexes[idx].description = "" /* to be filled */
		} else {
			info.indexes = append(info.indexes, indexinfo{repodir: repodir})
		}
	}

	slices.SortFunc(info.indexes, func(left, right indexinfo) int {
		return strings.Compare(left.repodir, right.repodir)
	})

	return &info
}
