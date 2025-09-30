#pragma once

#include <git2/patch.h>
#include <git2/types.h>

struct blobinfo {
	const char* name;
	const char* path;

	const git_blob* blob;
	uint32_t        hash;
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

struct gitininfo {
	char* cache;

	struct indexinfo* indexes;
	int               nindexes;
};

struct indexinfo {
	const char* repodir;
	const char* name;
	const char* description;

	struct repoinfo* repoinfo;
};

struct referenceinfo {
	git_reference* ref;
	char*          refname;
	git_commit*    commit;
	int            istag;
};

struct repoinfo {
	git_repository*      repo;
	struct referenceinfo branch;

	const char* repodir;
	int         relpath;

	char*       confbuffer;
	char        name[100];
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

int  getindex(struct gitininfo* info, const char** repos, int nrepos);
void freeindex(struct gitininfo* info);

/* refinfo */
int  getrefs(struct repoinfo* info);
int  getreference(struct referenceinfo* refinfo, git_reference* ref);
void freerefs(struct repoinfo* info);
void freereference(struct referenceinfo* refinfo);

/* repoinfo */
void getrepo(struct repoinfo* info, const char* repodir, int relpath);
void freerepo(struct repoinfo* info);
