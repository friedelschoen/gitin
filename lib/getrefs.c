#include "common.h"
#include "getinfo.h"
#include "hprintf.h"

#include <git2/commit.h>
#include <git2/refs.h>
#include <git2/types.h>
#include <string.h>


static int comparerefs(const void* v1, const void* v2) {
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

int getreference(struct referenceinfo* out, git_reference* ref) {
	git_reference* newref;

	if (!git_reference_is_branch(ref) && !git_reference_is_tag(ref)) {
		return -1;
	}

	if (git_reference_resolve(&newref, ref)) {
		return -1;
	}

	git_reference_free(ref);
	ref = newref;

	if (git_reference_peel((git_object**) &out->commit, ref, GIT_OBJECT_COMMIT))
		return -1;

	out->ref     = ref;
	out->refname = escaperefname(strdup(git_reference_shorthand(ref)));
	out->istag   = git_reference_is_tag(ref);
	return 0;
}

int getrefs(struct repoinfo* info) {
	git_reference_iterator* iter;
	git_reference*          ref;

	if (git_reference_iterator_new(&iter, info->repo))
		return -1;

	while (!git_reference_next(&ref, iter)) {
		if (!(info->refs = realloc(info->refs, (info->nrefs + 1) * sizeof(*info->refs)))) {
			hprintf(stderr, "error: unable to alloc memory for \"info->refs\": %w\n");
			continue;
		}
		if (getreference(&info->refs[info->nrefs], ref) == -1) {
			git_reference_free(ref);
			continue;
		}

		info->nrefs++;
	}
	git_reference_iterator_free(iter);

	/* sort by type, date then shorthand name */
	qsort(info->refs, info->nrefs, sizeof(*info->refs), comparerefs);

	return 0;
}

void freereference(struct referenceinfo* refinfo) {
	git_commit_free(refinfo->commit);
	git_reference_free(refinfo->ref);
	free(refinfo->refname);
}

void freerefs(struct repoinfo* info) {
	for (int i = 0; i < info->nrefs; i++) {
		freereference(&info->refs[i]);
	}
	free(info->refs);
}
