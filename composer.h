#pragma once

#include "getinfo.h"

int composefiletree(const struct repoinfo* info, git_reference* refname, git_commit* commit);

int composelog(const struct repoinfo* info, git_reference* ref, git_commit* commit);

void composerepo(const struct repoinfo* info);
