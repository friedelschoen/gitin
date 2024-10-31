#include "hprintf.h"
#include "writer.h"

#include <git2/commit.h>
#include <string.h>

#define ADDREF(refs, nrefs)                                                     \
	{                                                                           \
		if (!(refs = realloc(refs, (nrefs + 1) * sizeof(*refs)))) {             \
			hprintf(stderr, "error: unable to alloc memory for \"" #refs "\""); \
			continue;                                                           \
		}                                                                       \
                                                                                \
		refs[nrefs].ref    = ref;                                               \
		refs[nrefs].commit = commit;                                            \
		nrefs++;                                                                \
	}

static void freereference(struct referenceinfo* refs, int nrefs) {
	for (int i = 0; i < nrefs; i++) {
		git_commit_free(refs[i].commit);
		git_reference_free(refs[i].ref);
	}
	free(refs);
}

void freerefs(struct repoinfo* info) {
	freereference(info->branches, info->nbranches);
	freereference(info->tags, info->ntags);
}

static int refs_cmp(const void* v1, const void* v2) {
	const struct referenceinfo *r1 = v1, *r2 = v2;
	time_t                      t1, t2;

	t1 = git_commit_time(r1->commit);
	t2 = git_commit_time(r2->commit);

	if (t1 != t2)
		return t2 - t1;

	return strcmp(git_reference_shorthand(r1->ref), git_reference_shorthand(r2->ref));
}

int getrefs(struct repoinfo* info) {
	git_reference_iterator* iter;
	git_reference *         ref, *newref;
	git_commit*             commit;

	if (git_reference_iterator_new(&iter, info->repo))
		return -1;

	while (!git_reference_next(&ref, iter)) {
		if (!git_reference_is_branch(ref) && !git_reference_is_tag(ref)) {
			git_reference_free(ref);
			ref = NULL;
			continue;
		}

		if (git_reference_resolve(&newref, ref))
			continue;

		git_reference_free(ref);
		ref = newref;

		if (git_reference_peel((git_object**) &commit, ref, GIT_OBJECT_COMMIT))
			continue;

		if (git_reference_is_branch(ref)) {
			ADDREF(info->branches, info->nbranches);
		} else {    // is tag
			ADDREF(info->tags, info->ntags);
		}
	}
	git_reference_iterator_free(iter);

	/* sort by type, date then shorthand name */
	qsort(info->branches, info->nbranches, sizeof(*info->branches), refs_cmp);
	qsort(info->tags, info->ntags, sizeof(*info->tags), refs_cmp);

	return 0;
}
