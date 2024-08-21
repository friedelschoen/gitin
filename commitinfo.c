#include "commitinfo.h"

#include <err.h>
#include <memory.h>

#include "deltainfo.h"

int
commitinfo_getstats(struct commitinfo *ci, git_repository *repo)
{
	struct deltainfo *di;
	git_diff_options opts;
	git_diff_find_options fopts;
	const git_diff_delta *delta;
	const git_diff_hunk *hunk;
	const git_diff_line *line;
	git_patch *patch = NULL;
	size_t ndeltas, nhunks, nhunklines;
	size_t i, j, k;

	if (git_tree_lookup(&(ci->commit_tree), repo, git_commit_tree_id(ci->commit)))
		goto err;
	if (!git_commit_parent(&(ci->parent), ci->commit, 0)) {
		if (git_tree_lookup(&(ci->parent_tree), repo, git_commit_tree_id(ci->parent))) {
			ci->parent = NULL;
			ci->parent_tree = NULL;
		}
	}

	git_diff_init_options(&opts, GIT_DIFF_OPTIONS_VERSION);
	opts.flags |= GIT_DIFF_DISABLE_PATHSPEC_MATCH |
	              GIT_DIFF_IGNORE_SUBMODULES |
		      GIT_DIFF_INCLUDE_TYPECHANGE;
	if (git_diff_tree_to_tree(&(ci->diff), repo, ci->parent_tree, ci->commit_tree, &opts))
		goto err;

	if (git_diff_find_init_options(&fopts, GIT_DIFF_FIND_OPTIONS_VERSION))
		goto err;
	/* find renames and copies, exact matches (no heuristic) for renames. */
	fopts.flags |= GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES |
	               GIT_DIFF_FIND_EXACT_MATCH_ONLY;
	if (git_diff_find_similar(ci->diff, &fopts))
		goto err;

	ndeltas = git_diff_num_deltas(ci->diff);
	if (ndeltas && !(ci->deltas = calloc(ndeltas, sizeof(struct deltainfo *))))
		err(1, "calloc");

	for (i = 0; i < ndeltas; i++) {
		if (git_patch_from_diff(&patch, ci->diff, i))
			goto err;

		if (!(di = calloc(1, sizeof(struct deltainfo))))
			err(1, "calloc");
		di->patch = patch;
		ci->deltas[i] = di;

		delta = git_patch_get_delta(patch);

		/* skip stats for binary data */
		if (delta->flags & GIT_DIFF_FLAG_BINARY)
			continue;

		nhunks = git_patch_num_hunks(patch);
		for (j = 0; j < nhunks; j++) {
			if (git_patch_get_hunk(&hunk, &nhunklines, patch, j))
				break;
			for (k = 0; ; k++) {
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
	ci->ndeltas = i;
	ci->filecount = i;

	return 0;

err:
	git_diff_free(ci->diff);
	ci->diff = NULL;
	git_tree_free(ci->commit_tree);
	ci->commit_tree = NULL;
	git_tree_free(ci->parent_tree);
	ci->parent_tree = NULL;
	git_commit_free(ci->parent);
	ci->parent = NULL;

	if (ci->deltas)
		for (i = 0; i < ci->ndeltas; i++)
			deltainfo_free(ci->deltas[i]);
	free(ci->deltas);
	ci->deltas = NULL;
	ci->ndeltas = 0;
	ci->addcount = 0;
	ci->delcount = 0;
	ci->filecount = 0;

	return -1;
}

void
commitinfo_free(struct commitinfo *ci)
{
	size_t i;

	if (!ci)
		return;
	if (ci->deltas)
		for (i = 0; i < ci->ndeltas; i++)
			deltainfo_free(ci->deltas[i]);

	free(ci->deltas);
	git_diff_free(ci->diff);
	git_tree_free(ci->commit_tree);
	git_tree_free(ci->parent_tree);
	git_commit_free(ci->commit);
	git_commit_free(ci->parent);
	memset(ci, 0, sizeof(*ci));
	free(ci);
}

struct commitinfo *
commitinfo_getbyoid(const git_oid *id, git_repository *repo)
{
	struct commitinfo *ci;

	if (!(ci = calloc(1, sizeof(struct commitinfo))))
		err(1, "calloc");

	if (git_commit_lookup(&(ci->commit), repo, id))
		goto err;
	ci->id = id;

	git_oid_tostr(ci->oid, sizeof(ci->oid), git_commit_id(ci->commit));
	git_oid_tostr(ci->parentoid, sizeof(ci->parentoid), git_commit_parent_id(ci->commit, 0));

	ci->author = git_commit_author(ci->commit);
	ci->committer = git_commit_committer(ci->commit);
	ci->summary = git_commit_summary(ci->commit);
	ci->msg = git_commit_message(ci->commit);

	return ci;

err:
	commitinfo_free(ci);

	return NULL;
}
