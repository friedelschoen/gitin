#pragma once

#include <git2.h>
#include <stdio.h>

#define MAXPINS 8


struct repoinfo {
	git_repository* repo;

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

	/* cache */
	git_oid lastoid;
	char    lastoidstr[GIT_OID_HEXSZ + 2]; /* id + newline + NUL byte */
	FILE *  rcachefp, *wcachefp;
};

void writefooter(FILE* fp);
int  writefiles(FILE* fp, struct repoinfo* info, const git_oid* id);
int  writeindex(FILE* fp, const struct repoinfo* info);
int  writelog(FILE* fp, const struct repoinfo* info, const git_oid* oid);
void writeheader(FILE* fp, const struct repoinfo* info, int relpath, const char* name, const char* description);
void writerepo(FILE* index, const char* repodir);
