#pragma once

#include "getinfo.h"

#include <stdio.h>

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
void writeindex(const struct gitininfo* info);
int  writelog(const struct repoinfo* info, git_reference* ref, git_commit* commit);
void writepreview(FILE* fp, const struct repoinfo* info, int relpath, struct blobinfo* blob);
void writerepo(const struct repoinfo* info);
void writeredirect(FILE* fp, const char* to);
int  writerefs(FILE* fp, const struct repoinfo* info, int relpath, git_reference* current);
void writeshortlog(FILE* fp, const struct repoinfo* info, git_commit* head);
