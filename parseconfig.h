#pragma once

#include <stdio.h>

#define KEYMAX 128


struct configstate {
	char*  line;
	size_t len;

	char  key[KEYMAX];
	char* value;
};

int  parseconfig(struct configstate* state, FILE* fp);
void parseconfig_free(struct configstate* state);
