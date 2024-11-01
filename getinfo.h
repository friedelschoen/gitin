#pragma once

#include <git2/patch.h>
#include <git2/types.h>

struct blobinfo {
	const char* name;
	const char* path;

	const char* content;
	ssize_t     length;
	uint32_t    hash;
	int         is_binary;
};

struct commitinfo {
	size_t addcount;
	size_t delcount;

	struct deltainfo* deltas;
	size_t            ndeltas;
};

struct deltainfo {
	git_patch* patch;
	size_t     addcount;
	size_t     delcount;
};

struct indexinfo {
	const char* repodir;
	const char* name;
	const char* description;
};

struct referenceinfo {
	git_reference* ref;
	git_commit*    commit;
	int            istag;
};

struct repoinfo {
	git_repository* repo;
	git_reference*  branch;

	const char* repodir;
	char        destdir[1024];
	int         relpath;

	char*       confbuffer;
	char        name[100];
	const char* branchname;
	const char* description;
	const char* cloneurl;

	const char* submodules;

	const char* pinfiles[10];
	int         pinfileslen;

	char** headfiles;
	int    headfileslen;
	int    headfilesalloc;

	struct referenceinfo* refs;
	int                   nrefs;
};

/* commitinfo */
int  getdiff(struct commitinfo* ci, const struct repoinfo* info, git_commit* commit, int docache);
void freediff(struct commitinfo* ci);

/* refinfo */
int  getrefs(struct repoinfo* info);
void freerefs(struct repoinfo* info);

/* repoinfo */
void getrepo(struct repoinfo* info, const char* destination, const char* repodir);
void freeinfo(struct repoinfo* info);
