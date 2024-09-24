#include "hprintf.h"
#include "writer.h"

int writeindex(FILE* fp, const struct repoinfo* info) {
	git_commit*          commit = NULL;
	const git_signature* author;
	git_revwalk*         w = NULL;
	git_oid              id;
	int                  ret = 0;

	git_revwalk_new(&w, info->repo);
	if (git_revwalk_push_head(w)) {
		printf("unable to push head\n");
		return -1;
	}
	if (git_revwalk_next(&id, w) || git_commit_lookup(&commit, info->repo, &id)) {
		ret = -1;
		goto err;
	}

	author = git_commit_author(commit);

	hprintf(fp, "<tr><td><a href=\"%s/index.html\">%y</a></td><td>%y</td><td>", info->repodir, info->name,
	        info->description);
	if (author)
		hprintf(fp, "%t", &author->when);
	fputs("</td></tr>", fp);

	git_commit_free(commit);
err:
	git_revwalk_free(w);

	return ret;
}
