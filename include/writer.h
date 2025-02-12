#pragma once

#include "getinfo.h"

#include <stdio.h>

/* naming: write + <type if not html> + <what is writing> */
/* please order arguments: FILE* fp, (const) struct repoinfo* info, int relpath */
/* if a function writes a whole file, it must include writeheader() and writefooter() */

int writearchive(FILE* fp, const struct repoinfo* info, int type, struct referenceinfo* refinfo);

/* XML atom */
void writeatomfooter(FILE* fp);
void writeatomheader(FILE* fp, const struct repoinfo* info);
void writeatomrefs(FILE* fp, const struct repoinfo* info);
void writeatomcommit(FILE* fp, git_commit* commit, const char* tag);

/* JSON */
void writejsoncommit(FILE* fp, git_commit* commit, int first);
void writejsonrefs(FILE* fp, const struct repoinfo* info);

/* HTML */
void writecommit(FILE* fp, const struct repoinfo* info, git_commit* commit,
                 const struct commitinfo* ci, int parentlink);
void writefooter(FILE* fp);
void writeheader(FILE* fp, const struct repoinfo* info, int relpath, int inbranch, const char* name,
                 const char* description, ...);
void writeindex(FILE* fp, const struct gitininfo* info, int dorepo);
int  writelog(FILE* fp, FILE* atom, FILE* json, const struct repoinfo* info,
              struct referenceinfo* refinfo);
void writepreview(FILE* fp, const struct repoinfo* info, int relpath, struct blobinfo* blob,
                  int printplain);
void writeredirect(FILE* fp, const char* to, ...);
int  writerefs(FILE* fp, const struct repoinfo* info, int relpath, git_reference* current);
void writeshortlog(FILE* fp, const struct repoinfo* info, git_commit* head);
int  writesummary(FILE* fp, const struct repoinfo* info, struct referenceinfo* refinfo);
