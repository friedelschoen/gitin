package gitin

import (
	"encoding/json"
	"io"
	"log"
	"os"
	"path"

	git "github.com/jeffwelling/git2go/v37"
)

// #include "common.h"
// #include "getinfo.h"
// #include "fmt.Fprintf.h"

// #include <git2/commit.h>
// #include <string.h>

/* Function to dump the commitstats struct into a file */
func dumpdiff(fp io.Writer, stats commitinfo) error { // static void dumpdiff(io.Writer fp, const struct commitinfo* stats) {
	return json.NewEncoder(fp).Encode(stats)
}

/* Function to parse the commitstats struct from a file */
func loaddiff(fp io.Reader) (stats commitinfo, err error) { // static void loaddiff(io.Writer fp, struct commitinfo* stats) {
	err = json.NewDecoder(fp).Decode(&stats)
	return
}

func getdiff(info *repoinfo, commit *git.Commit, docache bool) (commitinfo, error) { // int getdiff(struct commitinfo* ci, const struct repoinfo* info, git.Commit* commit, int docache) {
	// var opts git.Diff_options       // git.Diff_options      opts;
	// var fopts git.Diff_find_options // git.Diff_find_options fopts;
	// var delta *git.Diff_delta       // const git.Diff_delta* delta;
	// var hunk *git.Diff_hunk         // const git.Diff_hunk*  hunk;
	// var line *git.Diff_line         // const git.Diff_line*  line;
	// var patch *git.Patch = nil      // git.Patch*            patch = NULL;
	// var nhunks uint64
	// var nhunklines uint64    // size_t                nhunks, nhunklines;
	// var diff *git.Diff = nil // git.Diff*             diff = NULL;
	// var parent *git.Commit   // git.Commit*           parent;
	// var commit_tree *git.Tree
	// var parent_tree *git.Tree              // git.Tree *            commit_tree, *parent_tree;
	// var fp io.Writer                    // io.Writer                 fp;
	// var oid [git.OID_SHA1_HEXSIZE + 1]byte // char                  oid[git.OID_SHA1_HEXSIZE + 1];

	os.MkdirAll(".cache/diffs", 0777)

	oid := commit.Id().String()
	// git.Oid_tostr(oid, unsafe.Sizeof(oid{}), git.Commit_id(commit)) // git.Oid_tostr(oid, sizeof(oid), git.Commit_id(commit));

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
		return commitinfo{}, err
	}

	parent := commit.Parent(0)
	var parentTree *git.Tree
	if parent != nil {
		parentTree, _ = parent.Tree()
	}

	diffopt, err := git.DefaultDiffOptions()
	if err != nil {
		return commitinfo{}, err
	}
	difffindopt, err := git.DefaultDiffFindOptions()
	if err != nil {
		return commitinfo{}, err
	}
	diffopt.Flags |= git.DiffDisablePathspecMatch | git.DiffIgnoreSubmodules
	difffindopt.Flags |= git.DiffFindRenames | git.DiffFindCopies | git.DiffFindExactMatchOnly

	diff, err := info.repo.DiffTreeToTree(parentTree, tree, &diffopt) // if (git.Diff_tree_to_tree(&diff, info->repo, parent_tree, commit_tree, &opts)) {
	if err != nil {
		return commitinfo{}, err
	}

	if err := diff.FindSimilar(&difffindopt); err != nil {
		return commitinfo{}, err

	}
	var ci commitinfo
	ci.diff = diff
	ndeltas, _ := diff.NumDeltas()
	ci.Deltas = make([]deltainfo, ndeltas)
	for i := range ndeltas {
		delta, err := diff.GetDelta(i)
		if err != nil {
			log.Printf("unable to load deltas %d of commit %s\n", i, oid)
		}
		ci.Deltas[i].delta = delta
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
			return commitinfo{}, err
		}
	}
	return ci, err
}
