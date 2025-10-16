package wrapper

import (
	"slices"
	"strings"

	"github.com/friedelschoen/gitin-go/internal/common"
	git "github.com/jeffwelling/git2go/v37"
)

type ReferenceInfo struct {
	Ref     *git.Reference
	Refname string
	Commit  *git.Commit
	IsTag   bool
}

func comparerefs(left, right *ReferenceInfo) int {
	if left.IsTag != right.IsTag {
		intbool := func(v bool) int {
			if v {
				return 1
			}
			return 0
		}
		return intbool(left.IsTag) - intbool(right.IsTag)
	}

	tleft := left.Commit.Committer().When
	tright := right.Commit.Committer().When

	if tleft != tright {
		return int(tright.UnixNano() - tleft.UnixNano())
	}
	return strings.Compare(left.Ref.Shorthand(), right.Ref.Shorthand())
}

func getreference(ref *git.Reference) (*ReferenceInfo, error) {
	if !ref.IsBranch() && !ref.IsTag() {
		return nil, nil
	}

	/* resolve reference */
	unref, err := ref.Resolve()
	if err != nil {
		return nil, err
	}

	commit, err := unref.Peel(git.ObjectCommit)
	if err != nil {
		return nil, err
	}

	var out ReferenceInfo
	out.Ref = unref
	out.Commit, _ = commit.AsCommit() /* ignoring error, we were peeling for a commit */
	out.Refname = common.Escaperefname(ref.Shorthand())
	out.IsTag = ref.IsTag()
	return &out, nil
}

func getrefs(repo *git.Repository) ([]*ReferenceInfo, error) {

	iter, err := repo.NewReferenceIterator()
	if err != nil {
		return nil, err
	}

	var out []*ReferenceInfo
	for {
		ref, err := iter.Next()
		if gerr, ok := err.(*git.GitError); ok && gerr.Code == git.ErrorCodeIterOver {
			break
		} else if err != nil {
			return nil, err
		}
		ri, err := getreference(ref)
		if err != nil {
			return nil, err
		}
		if ri != nil {
			out = append(out, ri)
		}
	}

	/* sort by type, date then shorthand name */
	slices.SortFunc(out, comparerefs)
	return out, nil
}
