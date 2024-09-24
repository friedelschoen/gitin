#pragma once

#include <git2.h>


struct deltainfo {
	git_patch* patch;
	size_t     addcount;
	size_t     delcount;
};

struct commitstats {
	git_commit* parent;
	git_tree*   commit_tree;
	git_tree*   parent_tree;

	git_diff* diff;

	size_t addcount;
	size_t delcount;
	size_t filecount;

	struct deltainfo** deltas;
	size_t             ndeltas;
};

int  commitinfo_getstats(struct commitstats* ci, git_commit* commit, git_repository* repo);
void commitinfo_free(struct commitstats* ci);
