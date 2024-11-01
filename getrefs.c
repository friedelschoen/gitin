#include "hprintf.h"
#include "writer.h"

#include <git2/commit.h>
#include <git2/refs.h>
#include <string.h>


void freerefs(struct repoinfo* info) {
	for (int i = 0; i < info->nrefs; i++) {
		git_commit_free(info->refs[i].commit);
		git_reference_free(info->refs[i].ref);
	}
	free(info->refs);
}

static int refs_cmp(const void* v1, const void* v2) {
	const struct referenceinfo *r1 = v1, *r2 = v2;
	time_t                      t1, t2;

	if (r1->istag != r2->istag)
		return r1->istag - r2->istag;

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

		if (!(info->refs = realloc(info->refs, (info->nrefs + 1) * sizeof(*info->refs)))) {
			hprintf(stderr, "error: unable to alloc memory for \"info->refs\": %w\n");
			continue;
		}
		info->refs[info->nrefs].ref    = ref;
		info->refs[info->nrefs].commit = commit;
		info->refs[info->nrefs].istag  = git_reference_is_tag(ref);
		info->nrefs++;
	}
	git_reference_iterator_free(iter);

	/* sort by type, date then shorthand name */
	qsort(info->refs, info->nrefs, sizeof(*info->refs), refs_cmp);

	return 0;
}
