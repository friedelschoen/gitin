#pragma once

#include <stdio.h>


struct configstate {
	char*  buffer;    // The entire config file
	size_t buflen;

	char* current;    // Pointer to the current position in the buffer
	char* key;        // Current key
	char* value;      // Current value
};

int  parseconfig(struct configstate* state, FILE* fp);
void parseconfig_free(struct configstate* state);
void setconfig(void);
