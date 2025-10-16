package wrapper

import (
	"iter"
	"slices"
	"strings"
)

type IndexInfo struct {
	Repodir     string `json:"repodir"`
	Name        string `json:"name"`
	Description string `json:"description"`
}

func GetIndex(repos iter.Seq[string]) []IndexInfo {
	var info []IndexInfo

	/* fill cache with to update repos */
	for repodir := range repos {
		if idx := slices.IndexFunc(info, func(ii IndexInfo) bool { return ii.Repodir == repodir }); idx != -1 {
			info[idx].Name = ""        /* to be filled */
			info[idx].Description = "" /* to be filled */
		} else {
			info = append(info, IndexInfo{Repodir: repodir})
		}
	}

	slices.SortFunc(info, func(left, right IndexInfo) int {
		return strings.Compare(left.Repodir, right.Repodir)
	})

	return info
}
