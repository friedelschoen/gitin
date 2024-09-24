#include "commit.h"

#include <err.h>
#include <stdio.h>
#include <string.h>


static int refs_cmp(const void* v1, const void* v2) {
	const struct referenceinfo *r1 = v1, *r2 = v2;
	time_t                      t1, t2;
	int                         r;
	const struct git_signature *author1, *author2;

	if ((r = git_reference_is_tag(r1->ref) - git_reference_is_tag(r2->ref)))
		return r;

	author1 = git_commit_author(r1->commit);
	author2 = git_commit_author(r2->commit);

	t1 = author1 ? author1->when.time : 0;
	t2 = author2 ? author2->when.time : 0;
	if ((r = t1 > t2 ? -1 : (t1 == t2 ? 0 : 1)))
		return r;

	return strcmp(git_reference_shorthand(r1->ref), git_reference_shorthand(r2->ref));
}

int getrefs(struct referenceinfo** pris, size_t* prefcount, git_repository* repo) {
	struct referenceinfo*   ris  = NULL;
	git_reference_iterator* it   = NULL;
	git_object*             obj  = NULL;
	git_reference *         dref = NULL, *ref = NULL;
	size_t                  i, refcount;

	*pris      = NULL;
	*prefcount = 0;

	if (git_reference_iterator_new(&it, repo))
		return -1;

	for (refcount = 0; !git_reference_next(&ref, it);) {
		if (!git_reference_is_branch(ref) && !git_reference_is_tag(ref)) {
			git_reference_free(ref);
			ref = NULL;
			continue;
		}

		if (git_reference_resolve(&dref, ref))
			goto err;

		if (git_reference_peel(&obj, dref, GIT_OBJECT_COMMIT))
			goto err;

		if (!(ris = realloc(ris, (refcount + 1) * sizeof(*ris))))
			err(1, "realloc");
		ris[refcount].commit = (git_commit*) obj;
		ris[refcount].ref    = dref;
		refcount++;
	}
	git_reference_iterator_free(it);

	/* sort by type, date then shorthand name */
	qsort(ris, refcount, sizeof(*ris), refs_cmp);

	*pris      = ris;
	*prefcount = refcount;

	return 0;

err:
	git_object_free(obj);
	git_reference_free(dref);
	for (i = 0; i < refcount; i++) {
		git_commit_free(ris[i].commit);
		git_reference_free(ris[i].ref);
	}
	free(ris);

	return -1;
}


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

	memset(ci, 0, sizeof(*ci));

	if (git_tree_lookup(&(ci->commit_tree), repo, git_commit_tree_id(commit)))
		goto err;
	if (!git_commit_parent(&(ci->parent), commit, 0)) {
		if (git_tree_lookup(&(ci->parent_tree), repo, git_commit_tree_id(ci->parent))) {
			ci->parent      = NULL;
			ci->parent_tree = NULL;
		}
	}

	git_diff_init_options(&opts, GIT_DIFF_OPTIONS_VERSION);
	opts.flags |= GIT_DIFF_DISABLE_PATHSPEC_MATCH | GIT_DIFF_IGNORE_SUBMODULES | GIT_DIFF_INCLUDE_TYPECHANGE;
	if (git_diff_tree_to_tree(&(ci->diff), repo, ci->parent_tree, ci->commit_tree, &opts))
		goto err;

	git_diff_find_init_options(&fopts, GIT_DIFF_FIND_OPTIONS_VERSION);
	fopts.flags |= GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES | GIT_DIFF_FIND_EXACT_MATCH_ONLY;
	if (git_diff_find_similar(ci->diff, &fopts))
		goto err;

	ndeltas = git_diff_num_deltas(ci->diff);
	if (ndeltas && !(ci->deltas = calloc(ndeltas, sizeof(struct deltainfo*))))
		err(1, "calloc");

	for (i = 0; i < ndeltas; i++) {
		if (git_patch_from_diff(&patch, ci->diff, i))
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

	return 0;

err:
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

	if (ci->diff)
		git_diff_free(ci->diff);
	if (ci->commit_tree)
		git_tree_free(ci->commit_tree);
	if (ci->parent_tree)
		git_tree_free(ci->parent_tree);

	git_commit_free(ci->parent);    // Free the parent commit
	memset(ci, 0, sizeof(*ci));     // Reset structure
}
