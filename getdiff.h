#pragma once

#include "writer.h"

int  getdiff(struct commitstats* ci, const struct repoinfo* info, git_commit* commit, int docache);
void freediff(struct commitstats* ci);
