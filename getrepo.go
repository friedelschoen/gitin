package gitin

import (
	"fmt"
	"os"
	"path"

	git "github.com/jeffwelling/git2go/v37"
)

func haspinfile(info *repoinfo, pinfile string) bool { // static void addpinfile(struct repoinfo* info, const char* pinfile) {
	obj, err := info.repo.RevparseSingle("HEAD:" + pinfile)
	if err != nil {
		/* it's okay, not everyone has a GUIDE_TO_BREAKING_PRODUCTION_IN_ONE_SIMPLE_STEP.md in their repository. */
		return false
	}
	return obj.Type() == git.ObjectBlob
}

func getrepo(repodir string, relpath int) (*repoinfo, error) { // void getrepo(struct repoinfo* info, const char* repodir, int relpath) {
	var info repoinfo
	// var obj *git.Object = nil // git.Object*    obj = NULL;
	// var fp io.Writer       // io.Writer          fp;
	// var start string
	// var reqbranchname string        // const char *   start, *reqbranchname = NULL;
	// var end *byte                   // char*          end;
	// var branch *git.Reference = nil // git.Reference* branch = NULL;

	info.repodir = repodir         // info->repodir     = repodir;
	info.relpath = relpath         // info->relpath = relpath;
	info.name = path.Base(repodir) // QUESTION: how many slashes in name, include user?

	var err error
	info.repo, err = git.OpenRepository(repodir)
	if err != nil {
		return nil, err
	}

	if file, err := os.Open(path.Join(info.repodir, config.Configfile)); err == nil { // if ((fp = efopen("!.r", "%s/%s", info->repodir, configfile))) {
		defer file.Close()
		for _, value := range ParseConfig(file, path.Join(info.repodir, config.Configfile)) {
			if err := UnmarshalConf(value, "", &info); err != nil {
				return nil, err
			}
		}
	}

	/* check pinfiles */
	for _, pf := range pinfiles { // for (int i = 0; pinfiles[i] && info->pinfileslen < (int) LEN(info->pinfiles); i++) {
		if haspinfile(&info, pf) {
			info.pinfiles = append(info.pinfiles, pf)
		}
	}

	if obj, err := info.repo.RevparseSingle("HEAD:.gitmodules"); err == nil && obj.Type() == git.ObjectBlob { // if (!git.Revparse_single(&obj, info->repo, "HEAD:.gitmodules")) {
		info.submodules = ".gitmodules" // info->submodules = ".gitmodules";
	}

	var branch *git.Reference
	if info.branchname != "" { // if (reqbranchname) {
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
