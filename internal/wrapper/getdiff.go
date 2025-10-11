package wrapper

import (
	"encoding/json"
	"io"
	"log"
	"os"
	"path"

	git "github.com/jeffwelling/git2go/v37"
)

/* Function to dump the commitstats struct into a file */
func dumpdiff(fp io.Writer, stats CommitInfo) error {
	return json.NewEncoder(fp).Encode(stats)
}

/* Function to parse the commitstats struct from a file */
func loaddiff(fp io.Reader) (stats CommitInfo, err error) {
	err = json.NewDecoder(fp).Decode(&stats)
	return
}

func GetDiff(info *RepoInfo, commit *git.Commit, docache bool) (CommitInfo, error) {

	os.MkdirAll(".cache/diffs", 0777)

	oid := commit.Id().String()

	if docache {
		if file, err := os.Open(path.Join(".cache/diffs", oid)); err == nil {
			defer file.Close()
			stats, err := loaddiff(file)
			if err != nil {
				return stats, nil
			}
		}
	}

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

	if file, err := os.Create(path.Join(".cache/diffs", oid)); err == nil {
		defer file.Close()
		if err := dumpdiff(file, ci); err != nil {
			return CommitInfo{}, err
		}
	}
	return ci, err
}
