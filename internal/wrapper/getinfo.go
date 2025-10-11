// package wrapper
package wrapper

import git "github.com/jeffwelling/git2go/v37"

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

type IndexInfo struct {
	Repodir     string `json:"repodir"`
	Name        string `json:"name"`
	Description string `json:"description"`

	repoinfo *RepoInfo
}

type ReferenceInfo struct {
	Ref     *git.Reference
	Refname string
	Commit  *git.Commit
	IsTag   bool
}

type RepoInfo struct {
	Repo   *git.Repository
	Branch *ReferenceInfo

	Repodir string
	Relpath int

	Name        string
	Description string `ini:"description"`
	Cloneurl    string `ini:"cloneurl,url"`
	Branchname  string `ini:"branch"`

	Submodules string

	Pinfiles  []string
	Headfiles []string

	Refs []*ReferenceInfo
}
