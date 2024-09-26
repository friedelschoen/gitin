#include "commit.h"

#include <err.h>
#include <string.h>

int commitinfo_getstats(struct commitstats* ci, git_commit* commit, git_repository* repo) {
	struct deltainfo*     di;
	git_diff_options      opts;
	git_diff_find_options fopts;
	const git_diff_delta* delta;
	const git_diff_hunk*  hunk;
	const git_diff_line*  line;
	git_patch*            patch = NULL;
	size_t                ndeltas, nhunks, nhunklines;
	size_t                i, j, k;

	git_commit* parent      = NULL;
	git_tree*   commit_tree = NULL;
	git_tree*   parent_tree = NULL;
	git_diff*   diff        = NULL;

	memset(ci, 0, sizeof(*ci));

	if (git_tree_lookup(&commit_tree, repo, git_commit_tree_id(commit)))
		goto err;
	if (!git_commit_parent(&parent, commit, 0)) {
		if (git_tree_lookup(&parent_tree, repo, git_commit_tree_id(parent))) {
			parent      = NULL;
			parent_tree = NULL;
		}
	}

	git_diff_options_init(&opts, GIT_DIFF_OPTIONS_VERSION);
	opts.flags |= GIT_DIFF_DISABLE_PATHSPEC_MATCH | GIT_DIFF_IGNORE_SUBMODULES | GIT_DIFF_INCLUDE_TYPECHANGE;
	if (git_diff_tree_to_tree(&diff, repo, parent_tree, commit_tree, &opts))
		goto err;

	git_diff_find_options_init(&fopts, GIT_DIFF_FIND_OPTIONS_VERSION);
	fopts.flags |= GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES | GIT_DIFF_FIND_EXACT_MATCH_ONLY;
	if (git_diff_find_similar(diff, &fopts))
		goto err;

	ndeltas = git_diff_num_deltas(diff);
	if (ndeltas && !(ci->deltas = calloc(ndeltas, sizeof(struct deltainfo*))))
		err(1, "calloc");

	for (i = 0; i < ndeltas; i++) {
		if (git_patch_from_diff(&patch, diff, i))
			goto err;

		if (!(di = calloc(1, sizeof(struct deltainfo))))
			err(1, "calloc");
		di->patch     = patch;
		ci->deltas[i] = di;

		delta = git_patch_get_delta(patch);

		if (delta->flags & GIT_DIFF_FLAG_BINARY)
			continue;

		nhunks = git_patch_num_hunks(patch);
		for (j = 0; j < nhunks; j++) {
			if (git_patch_get_hunk(&hunk, &nhunklines, patch, j))
				break;
			for (k = 0;; k++) {
				if (git_patch_get_line_in_hunk(&line, patch, j, k))
					break;
				if (line->old_lineno == -1) {
					di->addcount++;
					ci->addcount++;
				} else if (line->new_lineno == -1) {
					di->delcount++;
					ci->delcount++;
				}
			}
		}
	}
	ci->ndeltas   = i;
	ci->filecount = i;

	git_diff_free(diff);
	git_tree_free(commit_tree);
	git_tree_free(parent_tree);
	git_commit_free(parent);

	return 0;

err:
	git_diff_free(diff);
	git_tree_free(commit_tree);
	git_tree_free(parent_tree);
	git_commit_free(parent);
	commitinfo_free(ci);
	return -1;
}

void commitinfo_free(struct commitstats* ci) {
	size_t i;

	if (!ci)
		return;

	if (ci->deltas) {
		for (i = 0; i < ci->ndeltas; i++) {
			if (!ci->deltas[i])
				continue;
			git_patch_free(ci->deltas[i]->patch);
			free(ci->deltas[i]);
		}
		free(ci->deltas);
	}

	memset(ci, 0, sizeof(*ci));    // Reset structure
}
