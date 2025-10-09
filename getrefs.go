package gitin

import (
	"slices"
	"strings"

	git "github.com/jeffwelling/git2go/v37"
)

func comparerefs(left, right *referenceinfo) int {
	if left.istag != right.istag {
		intbool := func(v bool) int {
			if v {
				return 1
			}
			return 0
		}
		return intbool(left.istag) - intbool(right.istag)
	}

	tleft := left.commit.Committer().When
	tright := right.commit.Committer().When

	if tleft != tright {
		return int(tright.UnixNano() - tleft.UnixNano())
	}
	return strings.Compare(left.ref.Shorthand(), right.ref.Shorthand())
}

func getreference(ref *git.Reference) (*referenceinfo, error) {
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

	var out referenceinfo
	out.ref = unref
	out.commit, _ = commit.AsCommit() /* ignoring error, we were peeling for a commit */
	out.refname = escaperefname(ref.Shorthand())
	out.istag = ref.IsTag()
	return &out, nil
}

func getrefs(repo *git.Repository) ([]*referenceinfo, error) {

	iter, err := repo.NewReferenceIterator()
	if err != nil {
		return nil, err
	}

	var out []*referenceinfo
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
