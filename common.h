#pragma once

#define LEN(s) (sizeof(s) / sizeof(*s))


/* helper struct for pipe(2) */
typedef struct {
	int read;
	int write;
} pipe_t;

int  mkdirp(char* path, int mode);
int  removedir(char* path);
void normalize_path(char* path);
void unhide_path(char* path);
