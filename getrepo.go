package gitin

import (
	"fmt"
	"os"
	"path"

	git "github.com/jeffwelling/git2go/v37"
)

func haspinfile(info *repoinfo, pinfile string) bool {
	obj, err := info.repo.RevparseSingle("HEAD:" + pinfile)
	if err != nil {
		/* it's okay, not everyone has a GUIDE_TO_BREAKING_PRODUCTION_IN_ONE_SIMPLE_STEP.md in their repository. */
		return false
	}
	return obj.Type() == git.ObjectBlob
}

func getrepo(repodir string, relpath int) (*repoinfo, error) {
	var info repoinfo

	info.repodir = repodir
	info.relpath = relpath
	info.name = path.Base(repodir)

	var err error
	info.repo, err = git.OpenRepository(repodir)
	if err != nil {
		return nil, err
	}

	if file, err := os.Open(path.Join(info.repodir, config.Configfile)); err == nil {
		defer file.Close()
		for _, value := range ParseConfig(file, path.Join(info.repodir, config.Configfile)) {
			if err := UnmarshalConf(value, "", &info); err != nil {
				return nil, err
			}
		}
	}

	/* check pinfiles */
	for _, pf := range pinfiles {
		if haspinfile(&info, pf) {
			info.pinfiles = append(info.pinfiles, pf)
		}
	}

	if obj, err := info.repo.RevparseSingle("HEAD:.gitmodules"); err == nil && obj.Type() == git.ObjectBlob {
		info.submodules = ".gitmodules"
	}

	var branch *git.Reference
	if info.branchname != "" {
		branch, err = info.repo.References.Lookup(info.branchname)
		if err != nil {
			return nil, fmt.Errorf("unable to get branch %s: %w", info.branchname, err)
		}
	}
	if branch == nil {
		branch, err = info.repo.Head()
		if err != nil {
			return nil, fmt.Errorf("unable to get HEAD: %w", err)
		}
	}
	info.branch, err = getreference(branch)
	if err != nil {
		return nil, err
	}
	info.refs, err = getrefs(info.repo)
	if err != nil {
		return nil, err
	}
	return &info, nil
}
