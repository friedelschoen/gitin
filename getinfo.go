package gitin

import git "github.com/jeffwelling/git2go/v37"

// #pragma once

// #include <git2/patch.h>
// #include <git2/types.h>

type blobinfo struct {
	name string // const char* name;
	path string // const char* path;

	blob *git.Blob // const git.Blob* blob;
	hash uint32    // uint32_t        hash;
} // };

type commitinfo struct {
	diff     *git.Diff
	Addcount int `json:"addcount"` // size_t addcount;
	Delcount int `json:"delcount"` // size_t delcount;

	Deltas []deltainfo `json:"deltas"` // struct deltainfo* deltas;
} // };

type deltainfo struct {
	delta    git.DiffDelta `json:"-"`
	Addcount int           `json:"addcount"` // size_t     addcount;
	Delcount int           `json:"delcount"` // size_t     delcount;
} // };

type gitininfo struct {
	cache *byte // char* cache;

	indexes []indexinfo // struct indexinfo* indexes;
} // };

type indexinfo struct {
	repodir     string // const char* repodir;
	name        string // const char* name;
	description string // const char* description;

	repoinfo *repoinfo // struct repoinfo* repoinfo;
} // };

type referenceinfo struct {
	ref     *git.Reference // git.Reference* ref;
	refname string         // char*          refname;
	commit  *git.Commit    // git.Commit*    commit;
	istag   bool           // int            istag;
} // };

type repoinfo struct {
	repo   *git.Repository // git.Repository*      repo;
	branch *referenceinfo  // struct referenceinfo branch;

	repodir string // const char* repodir;
	relpath int    // int         relpath;

	name        string
	description string `ini:"description"`
	cloneurl    string `ini:"cloneurl,url"`
	branchname  string `ini:"branch"`

	submodules string // const char* submodules;

	pinfiles  []string
	headfiles []string

	refs []*referenceinfo // struct referenceinfo* refs;
} // };
