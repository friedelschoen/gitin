#include "writeindex.h"

#include "common.h"

#include <stdio.h>

extern const char* logoicon;
extern const char* faviconicon;
extern const char* stylesheet;

void writeheader_index(FILE* fp, const char* description) {
	fputs("<!DOCTYPE html>\n"
	      "<html>\n<head>\n"
	      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
	      "<title>",
	      fp);
	xmlencode(fp, description);
	fprintf(fp, "</title>\n<link rel=\"icon\" href=\"%s\" />\n", faviconicon);
	fprintf(fp, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s\" />\n", stylesheet);
	fputs("</head>\n<body>\n", fp);
	fprintf(fp,
	        "<table>\n<tr><td><img src=\"%s\" alt=\"\" width=\"32\" height=\"32\" /></td>\n"
	        "<td><span class=\"desc\">",
	        logoicon);
	xmlencode(fp, description);
	fputs("</span></td></tr><tr><td></td><td>\n"
	      "</td></tr>\n</table>\n<hr/>\n<div id=\"content\">\n"
	      "<table id=\"index\"><thead>\n"
	      "<tr><td><b>Name</b></td><td><b>Description</b></td><td><b>Last changes</b></td></tr>"
	      "</thead><tbody>\n",
	      fp);
}

void writefooter_index(FILE* fp) {
	fputs("</tbody>\n</table>\n</div>\n</body>\n</html>\n", fp);
}

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
