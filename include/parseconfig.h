#pragma once

#include <stdio.h>

enum {
	ConfigString,
	ConfigInteger,
	ConfigBoolean,
	ConfigBooleanAuto,
};

struct configitem {
	const char* name;
	int         type;
	void*       target;
};

char* parseconfig(FILE* file, struct configitem* keys);
