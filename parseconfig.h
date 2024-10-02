#pragma once

#include <stdio.h>

enum {
	ConfigString,
	ConfigInteger,
	ConfigBoolean,
};

struct config {
	const char* name;
	int         type;
	void*       target;
};

char* parseconfig(FILE* file, struct config* keys);
