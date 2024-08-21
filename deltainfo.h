#pragma once

#include <git2.h>

struct deltainfo {
	git_patch *patch;

	size_t addcount;
	size_t delcount;
};

void deltainfo_free(struct deltainfo *di);
