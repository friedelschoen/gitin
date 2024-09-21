#pragma once

#include <git2.h>
#include <stdio.h>

#define LEN(s) (sizeof(s) / sizeof(*s))


/* helper struct for pipe(2) */
typedef struct {
	int read;
	int write;
} pipe_t;

/* Handle read or write errors for a FILE * stream */
void checkfileerror(FILE* fp, const char* name, int mode);
int  mkdirp(const char* path);

/* Escape characters below as HTML 2.0 / XML 1.0, ignore printing '\r', '\n' */
void xmlencodeline(FILE* fp, const char* s, size_t len);

void normalize_path(char* path);
void unhide_path(char* path);
