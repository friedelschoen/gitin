#pragma once

#include <stdint.h>
#include <stdio.h>

struct executeinfo {
	const char*            command;
	FILE*                  fp;
	const struct repoinfo* info;
	const char*            filename;
	uint32_t               contenthash;
	const char*            cachename;
	const char*            content;
	ssize_t                ncontent;
	const char**           environ;
	int                    nenviron;
};

ssize_t execute(struct executeinfo* params);
