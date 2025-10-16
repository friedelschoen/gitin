// Package wrapper contains wrapper structs and methods around libgit2
package wrapper

import (
	"fmt"
	"os"
	"path"

	"github.com/BurntSushi/toml"
	"github.com/friedelschoen/gitin-go"
	git "github.com/jeffwelling/git2go/v37"
)

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

func haspinfile(info *RepoInfo, pinfile string) bool {
	obj, err := info.Repo.RevparseSingle("HEAD:" + pinfile)
	if err != nil {
		/* it's okay, not everyone has a GUIDE_TO_BREAKING_PRODUCTION_IN_ONE_SIMPLE_STEP.md in their repository. */
		return false
	}
	return obj.Type() == git.ObjectBlob
}

func Getrepo(repodir string, relpath int) (*RepoInfo, error) {
	var info RepoInfo

	info.Repodir = repodir
	info.Relpath = relpath
	info.Name = path.Base(repodir)

	var err error
	info.Repo, err = git.OpenRepository(repodir)
	if err != nil {
		return nil, err
	}

	if file, err := os.Open(path.Join(info.Repodir, gitin.Configfile)); err == nil {
		defer file.Close()
		if _, err := toml.NewDecoder(file).Decode(&info); err != nil {
			return nil, err
		}
	}

	/* check pinfiles */
	for _, pf := range gitin.Config.Pinfiles {
		if haspinfile(&info, pf) {
			info.Pinfiles = append(info.Pinfiles, pf)
		}
	}

	if obj, err := info.Repo.RevparseSingle("HEAD:.gitmodules"); err == nil && obj.Type() == git.ObjectBlob {
		info.Submodules = ".gitmodules"
	}

	var branch *git.Reference
	if info.Branchname != "" {
		branch, err = info.Repo.References.Lookup(info.Branchname)
		if err != nil {
			return nil, fmt.Errorf("unable to get branch %s: %w", info.Branchname, err)
		}
	}
	if branch == nil {
		branch, err = info.Repo.Head()
		if err != nil {
			return nil, fmt.Errorf("unable to get HEAD: %w", err)
		}
	}
	info.Branch, err = getreference(branch)
	if err != nil {
		return nil, err
	}
	info.Refs, err = getrefs(info.Repo)
	if err != nil {
		return nil, err
	}
	return &info, nil
}
