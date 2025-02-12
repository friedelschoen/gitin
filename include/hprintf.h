#pragma once

#include <stdio.h>

void hprintf(FILE* file, const char* format, ...);
void vhprintf(FILE* file, const char* format, va_list args);
