package wrapper

import (
	"errors"

	git "github.com/jeffwelling/git2go/v37"
)

var ErrNoBlob = errors.New("object not a blob")

func GetCommitBlob(repo *git.Repository, commit *git.Commit, path string) (*git.Blob, error) {
	tree, err := commit.Tree()
	if err != nil {
		return nil, err
	}

	entry, err := tree.EntryByPath(path)
	if err != nil {
		return nil, err
	}

	if entry.Type != git.ObjectBlob {
		return nil, ErrNoBlob
	}

	return repo.LookupBlob(entry.Id)
}
