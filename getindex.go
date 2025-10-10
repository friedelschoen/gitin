package gitin

import (
	"encoding/json"
	"fmt"
	"io"
	"iter"
	"os"
	"slices"
	"strings"
)

func parsecache(buffer io.Reader) (indexes []indexinfo, err error) {
	err = json.NewDecoder(buffer).Decode(&indexes)
	return
}

func GetIndex(repos iter.Seq[string]) []indexinfo {
	var info []indexinfo

	/* parse cache */
	if cachefp, err := os.Open(".cache/index"); err == nil {
		defer cachefp.Close()
		info, err = parsecache(cachefp)
		if err != nil {
			fmt.Fprintf(os.Stderr, "error: unable to load index-cache: %v\n", err)
		}
	}

	/* fill cache with to update repos */
	for repodir := range repos {
		if idx := slices.IndexFunc(info, func(ii indexinfo) bool { return ii.Repodir == repodir }); idx != -1 {
			info[idx].Name = ""        /* to be filled */
			info[idx].Description = "" /* to be filled */
		} else {
			info = append(info, indexinfo{Repodir: repodir})
		}
	}

	slices.SortFunc(info, func(left, right indexinfo) int {
		return strings.Compare(left.Repodir, right.Repodir)
	})

	return info
}
