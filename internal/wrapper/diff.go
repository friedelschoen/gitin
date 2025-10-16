package wrapper

import (
	"log"

	git "github.com/jeffwelling/git2go/v37"
)

type CommitInfo struct {
	Diff     *git.Diff
	Addcount int `json:"addcount"`
	Delcount int `json:"delcount"`

	Deltas []DeltaInfo `json:"deltas"`
}

type DeltaInfo struct {
	Delta    git.DiffDelta `json:"-"`
	Addcount int           `json:"addcount"`
	Delcount int           `json:"delcount"`
}

func GetDiff(info *RepoInfo, commit *git.Commit) (CommitInfo, error) {
	oid := commit.Id().String()

	tree, err := commit.Tree()
	if err != nil {
		return CommitInfo{}, err
	}

	parent := commit.Parent(0)
	var parentTree *git.Tree
	if parent != nil {
		parentTree, _ = parent.Tree()
	}

	diffopt, err := git.DefaultDiffOptions()
	if err != nil {
		return CommitInfo{}, err
	}
	difffindopt, err := git.DefaultDiffFindOptions()
	if err != nil {
		return CommitInfo{}, err
	}
	diffopt.Flags |= git.DiffDisablePathspecMatch | git.DiffIgnoreSubmodules
	difffindopt.Flags |= git.DiffFindRenames | git.DiffFindCopies | git.DiffFindExactMatchOnly

	diff, err := info.Repo.DiffTreeToTree(parentTree, tree, &diffopt)
	if err != nil {
		return CommitInfo{}, err
	}

	if err := diff.FindSimilar(&difffindopt); err != nil {
		return CommitInfo{}, err

	}
	var ci CommitInfo
	ci.Diff = diff
	ndeltas, _ := diff.NumDeltas()
	ci.Deltas = make([]DeltaInfo, ndeltas)
	for i := range ndeltas {
		delta, err := diff.GetDelta(i)
		if err != nil {
			log.Printf("unable to load deltas %d of commit %s\n", i, oid)
		}
		ci.Deltas[i].Delta = delta
		if delta.Flags&git.DiffFlagBinary > 0 {
			continue
		}
		s, _ := diff.Stats()
		ci.Deltas[i].Addcount = s.Insertions()
		ci.Deltas[i].Delcount = s.Deletions()
		ci.Addcount += s.Insertions()
		ci.Delcount += s.Deletions()
	}

	return ci, err
}
