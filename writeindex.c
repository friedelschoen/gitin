#include "writeindex.h"

#include "common.h"
#include "config.h"

#include <stdio.h>

int writelog_index(FILE* fp, const struct repoinfo* info) {
	git_commit*          commit = NULL;
	const git_signature* author;
	git_revwalk*         w = NULL;
	git_oid              id;
	int                  ret = 0;

	git_revwalk_new(&w, info->repo);
	git_revwalk_push_head(w);

	if (git_revwalk_next(&id, w) || git_commit_lookup(&commit, info->repo, &id)) {
		ret = -1;
		goto err;
	}

	author = git_commit_author(commit);

	fprintf(fp, "<tr><td><a href=\"%s/log.html\">", info->destdir);
	xmlencode(fp, info->name);
	fputs("</a></td><td>", fp);
	xmlencode(fp, info->description);
	fputs("</td><td>", fp);
	if (author)
		printtimeshort(fp, &(author->when));
	fputs("</td></tr>", fp);

	git_commit_free(commit);
err:
	git_revwalk_free(w);

	return ret;
}
