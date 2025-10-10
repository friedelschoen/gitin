// package gitin
package gitin

import git "github.com/jeffwelling/git2go/v37"

type blobinfo struct {
	name     string
	path     string
	binary   bool
	contents []byte
	hash     uint32
}

type commitinfo struct {
	diff     *git.Diff
	Addcount int `json:"addcount"`
	Delcount int `json:"delcount"`

	Deltas []deltainfo `json:"deltas"`
}

type deltainfo struct {
	delta    git.DiffDelta `json:"-"`
	Addcount int           `json:"addcount"`
	Delcount int           `json:"delcount"`
}

type indexinfo struct {
	Repodir     string `json:"repodir"`
	Name        string `json:"name"`
	Description string `json:"description"`

	repoinfo *repoinfo
}

type referenceinfo struct {
	ref     *git.Reference
	refname string
	commit  *git.Commit
	istag   bool
}

type repoinfo struct {
	repo   *git.Repository
	branch *referenceinfo

	repodir string
	relpath int

	name        string
	description string `ini:"description"`
	cloneurl    string `ini:"cloneurl,url"`
	branchname  string `ini:"branch"`

	submodules string

	pinfiles  []string
	headfiles []string

	refs []*referenceinfo
}
