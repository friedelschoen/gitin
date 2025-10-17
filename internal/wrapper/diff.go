package wrapper

import (
	"errors"
	"fmt"
	"log"
	"time"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	git "github.com/jeffwelling/git2go/v37"
)

type DeltaChange struct {
	OldLine int    `json:"old_line"`
	NewLine int    `json:"new_line"`
	Content string `json:"content"`
}

type Hunk struct {
	Header  string
	Changes []DeltaChange `json:"changes"`
}

type Signature struct {
	Valid bool
	Name  string
	Email string
	When  time.Time
}

type CommitInfo struct {
	ID        string      `json:"id"`
	ParentID  string      `json:"parent_id"`
	Parent    *CommitInfo `json:"-"`
	Author    Signature   `json:"author"`
	Committer Signature   `json:"committer"`
	Message   string      `json:"message"`
	Summary   string      `json:"summary"`
	Addcount  int         `json:"addcount"`
	Delcount  int         `json:"delcount"`
	Hunks     []*[]*Hunk  `json:"hunks"`
	Deltas    []DeltaInfo `json:"deltas"`
}

type DeltaInfo struct {
	Delta    git.DiffDelta `json:"-"`
	Addcount int           `json:"addcount"`
	Delcount int           `json:"delcount"`
}

func GetDiff(repo *git.Repository, commit *git.Commit, previous *CommitInfo) (*CommitInfo, error) {
	oid := commit.Id().String()

	tree, err := commit.Tree()
	if err != nil {
		return nil, err
	}

	parent := commit.Parent(0)
	var parentTree *git.Tree
	if parent != nil {
		parentTree, _ = parent.Tree()
	}

	diffopt, err := git.DefaultDiffOptions()
	if err != nil {
		return nil, err
	}
	difffindopt, err := git.DefaultDiffFindOptions()
	if err != nil {
		return nil, err
	}
	diffopt.Flags |= git.DiffDisablePathspecMatch | git.DiffIgnoreSubmodules
	difffindopt.Flags |= git.DiffFindRenames | git.DiffFindCopies | git.DiffFindExactMatchOnly

	diff, err := repo.DiffTreeToTree(parentTree, tree, &diffopt)
	if err != nil {
		return nil, err
	}

	if err := diff.FindSimilar(&difffindopt); err != nil {
		return nil, err

	}

	toSignature := func(gsig *git.Signature) (res Signature) {
		if gsig == nil {
			res.Valid = false
			return
		}
		res.Email = gsig.Email
		res.Name = gsig.Name
		res.When = gsig.When
		return
	}

	ci := &CommitInfo{}
	ci.ID = oid
	if previous != nil {
		previous.Parent = ci
		previous.ParentID = ci.ID
	}
	if p := commit.ParentId(0); p != nil {
		ci.ParentID = p.String()
	}
	ci.Author = toSignature(commit.Author())
	ci.Committer = toSignature(commit.Committer())
	ci.Message = commit.Message()
	ci.Summary = commit.Summary()

	err = diff.ForEach(func(file git.DiffDelta, progress float64) (git.DiffForEachHunkCallback, error) {
		var hks []*Hunk
		ci.Hunks = append(ci.Hunks, &hks)

		return func(hunk git.DiffHunk) (git.DiffForEachLineCallback, error) {
			var hk Hunk
			hks = append(hks, &hk)
			hk.Header = hunk.Header

			return func(line git.DiffLine) error {
				hk.Changes = append(hk.Changes, DeltaChange{
					OldLine: line.OldLineno,
					NewLine: line.NewLineno,
					Content: line.Content,
				})
				return nil
			}, nil
		}, nil
	}, git.DiffDetailLines)

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
		if s, err := diff.Stats(); err == nil {
			ci.Deltas[i].Addcount = s.Insertions()
			ci.Deltas[i].Delcount = s.Deletions()
			ci.Addcount += s.Insertions()
			ci.Delcount += s.Deletions()
		}
	}

	return ci, err
}

func GetCommits(repo *git.Repository, refinfo *ReferenceInfo, commitCache map[string]*CommitInfo) ([]*CommitInfo, int, error) {
	/* Create a revwalk to iterate through the commits */
	w, err := repo.Walk()
	if err != nil {
		return nil, 0, fmt.Errorf("unable to initialize revwalk: %w", err)
	}

	if err := w.Push(refinfo.Commit.Id()); err != nil {
		return nil, 0, err
	}
	ncommits := 0
	_ = w.Iterate(func(commit *git.Commit) bool {
		ncommits++
		return true
	})
	w.Reset()
	if err := w.Push(refinfo.Commit.Id()); err != nil {
		return nil, 0, err
	}

	msg := fmt.Sprintf("collect log: %s", refinfo.Refname)
	defer common.Printer.Done(msg)

	/* Iterate through the commits */
	indx := 0
	var (
		commits []*CommitInfo
		errs    []error

		previous *CommitInfo
	)
	err = w.Iterate(func(commit *git.Commit) bool {
		if gitin.Config.Maxcommits > 0 && indx >= gitin.Config.Maxcommits {
			return false
		}
		indx++

		oid := commit.Id().String()
		if ci, ok := commitCache[oid]; ok {
			for ci != nil {
				commits = append(commits, ci)
				ci = ci.Parent
			}
			return false
		}

		ci, err := GetDiff(repo, commit, previous)
		if err != nil {
			errs = append(errs, err)
			return true
		}
		commits = append(commits, ci)
		previous = ci

		common.Printer.Progress(msg, indx, ncommits)
		return true
	})
	if err != nil {
		return nil, 0, fmt.Errorf("error while iterating over commits: %w", err)
	}
	if err := errors.Join(errs...); err != nil {
		return nil, 0, err
	}
	return commits, ncommits, nil
}
