#include "commit.h"

#include <err.h>
#include <string.h>

static int refs_cmp(const void* v1, const void* v2) {
	const struct referenceinfo *r1 = v1, *r2 = v2;
	time_t                      t1, t2;
	int                         r;

	if ((r = git_reference_is_tag(r1->ref) - git_reference_is_tag(r2->ref)))
		return r;

	t1 = r1->ci->author ? r1->ci->author->when.time : 0;
	t2 = r2->ci->author ? r2->ci->author->when.time : 0;
	if ((r = t1 > t2 ? -1 : (t1 == t2 ? 0 : 1)))
		return r;

	return strcmp(git_reference_shorthand(r1->ref), git_reference_shorthand(r2->ref));
}

int getrefs(struct referenceinfo** pris, size_t* prefcount, git_repository* repo) {
	struct referenceinfo*   ris  = NULL;
	struct commitinfo*      ci   = NULL;
	git_reference_iterator* it   = NULL;
	const git_oid*          id   = NULL;
	git_object*             obj  = NULL;
	git_reference *         dref = NULL, *r, *ref = NULL;
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

		switch (git_reference_type(ref)) {
			case GIT_REF_SYMBOLIC:
				if (git_reference_resolve(&dref, ref))
					goto err;
				r = dref;
				break;
			case GIT_REF_OID:
				r = ref;
				break;
			default:
				continue;
		}
		if (!git_reference_target(r) || git_reference_peel(&obj, r, GIT_OBJ_ANY))
			goto err;
		if (!(id = git_object_id(obj)))
			goto err;
		if (!(ci = commitinfo_getbyoid(id, repo)))
			break;

		if (!(ris = realloc(ris, (refcount + 1) * sizeof(*ris))))
			err(1, "realloc");
		ris[refcount].ci  = ci;
		ris[refcount].ref = r;
		refcount++;

		git_object_free(obj);
		obj = NULL;
		git_reference_free(dref);
		dref = NULL;
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
	commitinfo_free(ci);
	for (i = 0; i < refcount; i++) {
		commitinfo_free(ris[i].ci);
		git_reference_free(ris[i].ref);
	}
	free(ris);

	return -1;
}

void deltainfo_free(struct deltainfo* di) {
	if (!di)
		return;
	git_patch_free(di->patch);
	memset(di, 0, sizeof(*di));
	free(di);
}

int commitinfo_getstats(struct commitinfo* ci, git_repository* repo) {
	struct deltainfo*     di;
	git_diff_options      opts;
	git_diff_find_options fopts;
	const git_diff_delta* delta;
	const git_diff_hunk*  hunk;
	const git_diff_line*  line;
	git_patch*            patch = NULL;
	size_t                ndeltas, nhunks, nhunklines;
	size_t                i, j, k;

	if (git_tree_lookup(&(ci->commit_tree), repo, git_commit_tree_id(ci->commit)))
		goto err;
	if (!git_commit_parent(&(ci->parent), ci->commit, 0)) {
		if (git_tree_lookup(&(ci->parent_tree), repo, git_commit_tree_id(ci->parent))) {
			ci->parent      = NULL;
			ci->parent_tree = NULL;
		}
	}

	git_diff_init_options(&opts, GIT_DIFF_OPTIONS_VERSION);
	opts.flags |= GIT_DIFF_DISABLE_PATHSPEC_MATCH | GIT_DIFF_IGNORE_SUBMODULES | GIT_DIFF_INCLUDE_TYPECHANGE;
	if (git_diff_tree_to_tree(&(ci->diff), repo, ci->parent_tree, ci->commit_tree, &opts))
		goto err;

	if (git_diff_find_init_options(&fopts, GIT_DIFF_FIND_OPTIONS_VERSION))
		goto err;
	/* find renames and copies, exact matches (no heuristic) for renames. */
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

		/* skip stats for binary data */
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
	ci->deltas    = NULL;
	ci->ndeltas   = 0;
	ci->addcount  = 0;
	ci->delcount  = 0;
	ci->filecount = 0;

	return -1;
}

void commitinfo_free(struct commitinfo* ci) {
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

struct commitinfo* commitinfo_getbyoid(const git_oid* id, git_repository* repo) {
	struct commitinfo* ci;

	if (!(ci = calloc(1, sizeof(struct commitinfo))))
		err(1, "calloc");

	if (git_commit_lookup(&(ci->commit), repo, id))
		goto err;
	ci->id = id;

	git_oid_tostr(ci->oid, sizeof(ci->oid), git_commit_id(ci->commit));
	git_oid_tostr(ci->parentoid, sizeof(ci->parentoid), git_commit_parent_id(ci->commit, 0));

	ci->author    = git_commit_author(ci->commit);
	ci->committer = git_commit_committer(ci->commit);
	ci->summary   = git_commit_summary(ci->commit);
	ci->msg       = git_commit_message(ci->commit);

	return ci;

err:
	commitinfo_free(ci);

	return NULL;
}
