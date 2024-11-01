#pragma once

#include <git2/patch.h>
#include <git2/types.h>
#include <stdio.h>

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

struct referenceinfo {
	git_reference* ref;
	git_commit*    commit;
	int            istag;
};

struct indexinfo {
	const char* repodir;
	const char* name;
	const char* description;
};

struct blobinfo {
	const char* name;
	const char* path;

	const char* content;
	ssize_t     length;
	uint32_t    hash;
	int         is_binary;
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

/* diffs/refs */
int  getdiff(struct commitinfo* ci, const struct repoinfo* info, git_commit* commit, int docache);
void freediff(struct commitinfo* ci);
int  getrefs(struct repoinfo* info);
void freerefs(struct repoinfo* info);
void getrepo(struct repoinfo* info, const char* destination, const char* repodir);
void freeinfo(struct repoinfo* info);

/* please order arguments: FILE* fp, (const) struct repoinfo* info, int relpath */
/* naming: write + <type if not html> + <what is writing> */

/* archive */
int writearchive(const struct repoinfo* info, int type, git_reference* refname, git_commit* commit);

/* XML atom */
void writeatomfooter(FILE* fp);
void writeatomheader(FILE* fp, const struct repoinfo* info);
void writeatomrefs(FILE* fp, const struct repoinfo* info);
void writeatomcommit(FILE* fp, git_commit* commit, git_reference* tag);

/* JSON */
void writejsoncommit(FILE* fp, git_commit* commit, int first);
void writejsonrefs(FILE* fp, const struct repoinfo* info);

/* HTML */
void writecommit(FILE* fp, const struct repoinfo* info, git_commit* commit,
                 const struct commitinfo* ci, int parentlink, const char* refname);
void writefooter(FILE* fp);
int  writefiletree(const struct repoinfo* info, git_reference* refname, git_commit* commit);
void writeheader(FILE* fp, const struct repoinfo* info, int relpath, const char* name,
                 const char* description, ...);
void writeindex(const char* destdir, char** repos, int nrepos);
int  writelog(const struct repoinfo* info, git_reference* ref, git_commit* commit);
void writepreview(FILE* fp, const struct repoinfo* info, int relpath, struct blobinfo* blob);
void writerepo(const struct repoinfo* info);
void writeredirect(FILE* fp, const char* to);
int  writerefs(FILE* fp, const struct repoinfo* info, int relpath, git_reference* current);
void writeshortlog(FILE* fp, const struct repoinfo* info, git_commit* head);
