#pragma once

#include <git2/patch.h>
#include <git2/types.h>
#include <stdio.h>

#define MAXPINS 8
#define SUMMARY 5

struct deltainfo {
	git_patch* patch;
	size_t     addcount;
	size_t     delcount;
};

struct commitstats {
	size_t addcount;
	size_t delcount;

	struct deltainfo* deltas;
	size_t            ndeltas;
};

struct repoinfo {
	git_repository* repo;
	const git_oid*  head;

	const char* repodir;
	char        destdir[1024];
	int         relpath;

	char        name[100];
	const char* description;
	const char* cloneurl;

	const char* submodules;

	const char* pinfiles[MAXPINS];
	int         pinfileslen;

	char** headfiles;
	int    headfileslen;
	int    headfilesalloc;
};

int  writearchive(const struct repoinfo* info, const struct git_reference* ref);
void writeatomheader(FILE* fp, const struct repoinfo* info);
void writeatomfooter(FILE* fp);
void writecommitatom(FILE* fp, git_commit* commit, const char* tag);
void writecommitfile(const struct repoinfo* info, git_commit* commit, const struct commitstats* ci,
                     int parentlink);
void writefooter(FILE* fp);
int  writefiles(struct repoinfo* info);
void writeindex(const char* destdir, char** repos, int nrepos);
int  writeindexline(FILE* fp, const struct repoinfo* info);
void writejsoncommit(FILE* fp, git_commit* commit, int first);
void writejsonref(FILE* fp, const struct repoinfo* info, git_reference* ref, git_commit* commit);
int  writelog(const struct repoinfo* info);
void writeheader(FILE* fp, const struct repoinfo* info, int relpath, const char* name,
                 const char* description, ...);
int  writerefs(FILE* fp, FILE* json, const struct repoinfo* info);
void writerepo(FILE* index, const char* repodir, const char* destination);
