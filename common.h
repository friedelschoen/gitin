#pragma once

#include <stdio.h>

#define LEN(s) (sizeof(s) / sizeof(*s))


/* helper struct for pipe(2) */
typedef struct {
	int read;
	int write;
} pipe_t;

int mkdirp(char* path, int mode);
int removedir(char* path);

/* Escape characters below as HTML 2.0 / XML 1.0, ignore printing '\r', '\n' */
void xmlencodeline(FILE* fp, const char* s, size_t len);

void normalize_path(char* path);
void unhide_path(char* path);
