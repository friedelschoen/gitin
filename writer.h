#pragma once

#include <git2.h>
#include <stdio.h>

#define MAXPINS 8
#define SUMMARY 5

struct repoinfo {
	git_repository* repo;
	const git_oid*  head;

	const char* repodir;
	char        destdir[1024];
	int         relpath;

	char name[100];
	char description[255];
	char cloneurl[255];

	const char* submodules;

	const char* pinfiles[MAXPINS];
	int         pinfileslen;

	char** headfiles;
	int    headfileslen;
	int    headfilesalloc;
};

struct deltainfo {
	git_patch* patch;
	size_t     addcount;
	size_t     delcount;
};

struct commitstats {
	size_t addcount;
	size_t delcount;
	size_t filecount;

	struct deltainfo** deltas;
	size_t             ndeltas;
};

int  writearchive(const struct repoinfo* info, const struct git_reference* ref);
void writeatomheader(FILE* fp, const struct repoinfo* info);
void writeatomfooter(FILE* fp);
void writecommitatom(FILE* fp, git_commit* commit, const char* tag);
void writediff(FILE* fp, const struct repoinfo* info, git_commit* commit, struct commitstats* ci, int parentlink);
void writefooter(FILE* fp);
int  writefiles(struct repoinfo* info);
void writeindex(const char* destdir, char** repos, int nrepos);
int  writeindexline(FILE* fp, const struct repoinfo* info);
int  writelog(FILE* fp, const struct repoinfo* info);
void writeheader(FILE* fp, const struct repoinfo* info, int relpath, const char* name, const char* description, ...);
int  writerefs(FILE* fp, const struct repoinfo* info);
void writerepo(FILE* index, const char* repodir, const char* destination);
