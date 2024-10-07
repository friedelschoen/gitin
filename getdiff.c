#include "getdiff.h"

#include "hprintf.h"

#include <git2/commit.h>
#include <string.h>


int getdiff(struct commitstats* ci, git_repository* repo, git_commit* commit) {
	git_diff_options      opts;
	git_diff_find_options fopts;
	const git_diff_delta* delta;
	const git_diff_hunk*  hunk;
	const git_diff_line*  line;
	git_patch*            patch = NULL;
	size_t                ndeltas, nhunks, nhunklines;
	size_t                i, j, k;
	git_diff*             diff = NULL;
	git_commit*           parent;
	git_tree *            commit_tree, *parent_tree;

	if (git_commit_tree(&commit_tree, commit)) {
		hprintf(stderr, "error: unable to fetch tree: %gw");
		return -1;
	}

	if (git_commit_parent(&parent, commit, 0)) {
		parent      = NULL;
		parent_tree = NULL;
	} else if (git_commit_tree(&parent_tree, parent)) {
		hprintf(stderr, "warn: unable to fetch parent-tree: %gw");
		git_commit_free(parent);
		parent      = NULL;
		parent_tree = NULL;
	}

	git_diff_options_init(&opts, GIT_DIFF_OPTIONS_VERSION);
	git_diff_find_options_init(&fopts, GIT_DIFF_FIND_OPTIONS_VERSION);
	opts.flags |=
	    GIT_DIFF_DISABLE_PATHSPEC_MATCH | GIT_DIFF_IGNORE_SUBMODULES | GIT_DIFF_INCLUDE_TYPECHANGE;
	fopts.flags |= GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES | GIT_DIFF_FIND_EXACT_MATCH_ONLY;

	memset(ci, 0, sizeof(*ci));

	if (git_diff_tree_to_tree(&diff, repo, parent_tree, commit_tree, &opts)) {
		hprintf(stderr, "error: unable to generate diff: %gw\n");
		goto err;
	}

	if (git_diff_find_similar(diff, &fopts)) {
		hprintf(stderr, "error: unable to find similar changes: %gw\n");
		goto err;
	}

	ndeltas = git_diff_num_deltas(diff);
	if (ndeltas && !(ci->deltas = calloc(ndeltas, sizeof(struct deltainfo)))) {
		hprintf(stderr, "error: unable to allocate memory for deltas: %w\n");
		exit(100);    // Fatal error
	}

	for (i = 0; i < ndeltas; i++) {
		if (git_patch_from_diff(&patch, diff, i)) {
			hprintf(stderr, "error: unable to create patch from diff: %gw\n");
			goto err;
		}

		ci->deltas[i].patch    = patch;
		ci->deltas[i].addcount = 0;
		ci->deltas[i].delcount = 0;

		delta = git_patch_get_delta(patch);

		if (delta->flags & GIT_DIFF_FLAG_BINARY)
			continue;

		nhunks = git_patch_num_hunks(patch);
		for (j = 0; j < nhunks; j++) {
			if (git_patch_get_hunk(&hunk, &nhunklines, patch, j)) {
				hprintf(stderr, "error: unable to get hunk: %gw\n");
				break;
			}
			for (k = 0;; k++) {
				if (git_patch_get_line_in_hunk(&line, patch, j, k))
					break;
				if (line->old_lineno == -1) {
					ci->deltas[i].addcount++;
					ci->addcount++;
				} else if (line->new_lineno == -1) {
					ci->deltas[i].delcount++;
					ci->delcount++;
				}
			}
		}
	}
	ci->ndeltas   = i;
	ci->filecount = i;

	git_diff_free(diff);
	return 0;

err:
	git_diff_free(diff);
	freediff(ci);
	return -1;
}

void freediff(struct commitstats* ci) {
	size_t i;

	if (!ci)
		return;

	if (ci->deltas) {
		for (i = 0; i < ci->ndeltas; i++) {
			git_patch_free(ci->deltas[i].patch);
		}
		free(ci->deltas);
	}

	memset(ci, 0, sizeof(*ci));    // Reset structure
}
