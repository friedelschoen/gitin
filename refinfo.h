#pragma once

#include <git2.h>

/* reference and associated data for sorting */
struct referenceinfo {
	struct git_reference *ref;
	struct commitinfo *ci;
};

int refs_cmp(const void *v1, const void *v2);
int getrefs(struct referenceinfo **pris, size_t *prefcount, git_repository *repo);