
#include <git2/patch.h>

struct deltainfo {
	git_patch* patch;
	size_t     addcount;
	size_t     delcount;
};

struct commitstats {
	size_t addcount;
	size_t delcount;
	size_t filecount;

	struct deltainfo* deltas;
	size_t            ndeltas;
};

int  getdiff(struct commitstats* ci, git_repository* repo, git_commit* commit);
void freediff(struct commitstats* ci);
