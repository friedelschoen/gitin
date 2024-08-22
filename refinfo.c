#include "refinfo.h"

#include <err.h>
#include <string.h>

#include "commitinfo.h"

static int
refs_cmp(const void *v1, const void *v2)
{
	const struct referenceinfo *r1 = v1, *r2 = v2;
	time_t t1, t2;
	int r;

	if ((r = git_reference_is_tag(r1->ref) - git_reference_is_tag(r2->ref)))
		return r;

	t1 = r1->ci->author ? r1->ci->author->when.time : 0;
	t2 = r2->ci->author ? r2->ci->author->when.time : 0;
	if ((r = t1 > t2 ? -1 : (t1 == t2 ? 0 : 1)))
		return r;

	return strcmp(git_reference_shorthand(r1->ref),
	              git_reference_shorthand(r2->ref));
}

int
getrefs(struct referenceinfo **pris, size_t *prefcount, git_repository *repo)
{
	struct referenceinfo *ris = NULL;
	struct commitinfo *ci = NULL;
	git_reference_iterator *it = NULL;
	const git_oid *id = NULL;
	git_object *obj = NULL;
	git_reference *dref = NULL, *r, *ref = NULL;
	size_t i, refcount;

	*pris = NULL;
	*prefcount = 0;

	if (git_reference_iterator_new(&it, repo))
		return -1;

	for (refcount = 0; !git_reference_next(&ref, it); ) {
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
		if (!git_reference_target(r) ||
		    git_reference_peel(&obj, r, GIT_OBJ_ANY))
			goto err;
		if (!(id = git_object_id(obj)))
			goto err;
		if (!(ci = commitinfo_getbyoid(id, repo)))
			break;

		if (!(ris = realloc(ris, (refcount + 1) * sizeof(*ris))))
			err(1, "realloc");
		ris[refcount].ci = ci;
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

	*pris = ris;
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
