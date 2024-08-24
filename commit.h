#pragma once

#include <git2.h>

struct referenceinfo {
	struct git_reference *ref;
	struct commitinfo *ci;
};

struct deltainfo {
	git_patch *patch;

	size_t addcount;
	size_t delcount;
};

struct commitinfo {
	const git_oid *id;

	char oid[GIT_OID_HEXSZ + 1];
	char parentoid[GIT_OID_HEXSZ + 1];

	const git_signature *author;
	const git_signature *committer;
	const char          *summary;
	const char          *msg;

	git_diff   *diff;
	git_commit *commit;
	git_commit *parent;
	git_tree   *commit_tree;
	git_tree   *parent_tree;

	size_t addcount;
	size_t delcount;
	size_t filecount;

	struct deltainfo **deltas;
	size_t ndeltas;
};

int getrefs(struct referenceinfo **pris, size_t *prefcount, git_repository *repo);

void deltainfo_free(struct deltainfo *di);

int commitinfo_getstats(struct commitinfo *ci, git_repository *repo);
void commitinfo_free(struct commitinfo *ci);
struct commitinfo* commitinfo_getbyoid(const git_oid *id, git_repository *repo);