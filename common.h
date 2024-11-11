#pragma once

#include "macro.h"

#include <stdint.h>
#include <stdio.h>
typedef struct {
	int read;
	int write;
} pipe_t;

int         isprefix(const char* str, const char* prefix);
int         issuffix(const char* str, const char* suffix);
void        printprogress(ssize_t indx, ssize_t ncommits, const char* what, ...) FORMAT(3, 4);
int         removedir(char* path);
const char* splitunit(ssize_t* size);
FILE*       efopen(const char* mode, const char* format, ...) FORMAT(2, 3);
void        emkdirf(const char* format, ...) FORMAT(1, 2);
uint32_t    filehash(const void* key, int len);
char*       escaperefname(char* refname);
