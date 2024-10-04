#pragma once

#define LEN(s) (sizeof(s) / sizeof(*s))

#include <stdio.h>

/* helper struct for pipe(2) */
typedef struct {
	int read;
	int write;
} pipe_t;

FILE* xfopen(const char* mode, const char* format, ...) __attribute__((format(printf, 2, 3)));
void  xmkdirf(int mode, const char* format, ...) __attribute__((format(printf, 2, 3)));
int   removedir(char* path);
void  normalize_path(char* path);
void  unhide_path(char* path);
