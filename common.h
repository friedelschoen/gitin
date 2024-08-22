#pragma once

#include <git2.h>
#include <stdio.h>

#define LEN(s)      (sizeof(s)/sizeof(*s))


/* helper struct for pipe(2) */
typedef struct { int read; int write; } pipe_t;

/* Handle read or write errors for a FILE * stream */
void checkfileerror(FILE *fp, const char *name, int mode);
int mkdirp(const char *path);
void printtimez(FILE *fp, const git_time *intime);
void printtime(FILE *fp, const git_time *intime);
void printtimeshort(FILE *fp, const git_time *intime);
