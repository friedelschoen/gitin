#pragma once

#include "macro.h"

#include <stdio.h>

/* loading/writing buffer */
int   loadbuffer(char* buffer, size_t len, const char* format, ...) FORMAT(3, 4);
char* loadbuffermalloc(FILE* fp, size_t* pbuflen);
int   writebuffer(const char* buffer, size_t len, const char* format, ...) FORMAT(3, 4);
