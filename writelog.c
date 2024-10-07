#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "writer.h"

#include <git2/commit.h>
#include <git2/revwalk.h>
#include <string.h>


static void writelogline(FILE* fp, git_commit* commit, const struct commitstats* ci) {
	char                 oid[GIT_OID_SHA1_HEXSIZE + 1];
	const git_signature* author  = git_commit_author(commit);
	const char*          summary = git_commit_summary(commit);

	git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));

	fputs("<tr><td>", fp);
	if (author)
		hprintf(fp, "%t", &author->when);
	fputs("</td><td>", fp);
	if (summary) {
		hprintf(fp, "<a href=\"commit/%s.html\">%y</a>", oid, summary);
	}
	fputs("</td><td>", fp);
	if (author)
		hprintf(fp, "%y", author->name);
	fputs("</td><td class=\"num\" align=\"right\">", fp);
	fprintf(fp, "%zu", ci->filecount);
	fputs("</td><td class=\"num\" align=\"right\">", fp);
	fprintf(fp, "+%zu", ci->addcount);
	fputs("</td><td class=\"num\" align=\"right\">", fp);
	fprintf(fp, "-%zu", ci->delcount);
	fputs("</td></tr>\n", fp);
}

static void writelogcommit(FILE* fp, FILE* json, FILE* atom, const struct repoinfo* info, int index,
                           git_commit* commit) {

	struct commitstats ci;

	memset(&ci, 0, sizeof(ci));

	if (getdiff(&ci, info->repo, commit) == -1)
		return;

	writecommitfile(info, commit, &ci, index == maxcommits);

	writelogline(fp, commit, &ci);
	writecommitatom(atom, commit, NULL);
	writejsoncommit(json, commit, index == 0);

	freediff(&ci);
	git_commit_free(commit);
}

int writelog(const struct repoinfo* info) {
	git_revwalk* w = NULL;
	git_oid      id;
	ssize_t      ncommits = 0;
	FILE *       fp, *atom, *json;

	/* log for HEAD */
	fp   = xfopen("w", "%s/index.html", info->destdir);
	json = xfopen("w", "%s/%s", info->destdir, jsonfile);
	atom = xfopen("w", "%s/%s", info->destdir, commitatomfile);

	writeheader(fp, info, 0, info->name, "%y", info->description);
	fprintf(json, "{");
	writerefs(fp, json, info);

	fputs("<h2>Commits</h2>\n<table id=\"log\"><thead>\n<tr><td><b>Date</b></td>"
	      "<td class=\"expand\"><b>Commit message</b></td>"
	      "<td><b>Author</b></td><td class=\"num\" align=\"right\"><b>Files</b></td>"
	      "<td class=\"num\" align=\"right\"><b>+</b></td>"
	      "<td class=\"num\" align=\"right\"><b>-</b></td></tr>\n</thead><tbody>\n",
	      fp);

	fprintf(json, ",\"commits\":{");

	writeatomheader(atom, info);

	// Create a revwalk to iterate through the commits
	if (git_revwalk_new(&w, info->repo)) {
		hprintf(stderr, "error: unable to initialize revwalk: %gw\n");
		return -1;
	}
	git_revwalk_push_head(w);

	git_commit* current;
	git_tree*   current_tree = NULL;

	// Iterate through the commits
	while (!git_revwalk_next(&id, w)) {
		if (maxcommits > 0 && ncommits > maxcommits) {
			ncommits++;    // we still want to know how many commits we have
			continue;
		}

		// Lookup the current commit
		if (git_commit_lookup(&current, info->repo, &id)) {
			hprintf(stderr, "error: unable to lookup commit: %gw\n");
			continue;
		}

		// Get the tree for the current commit
		if (git_commit_tree(&current_tree, current)) {
			hprintf(stderr, "error: unable to get tree for commit: %gw\n");
			git_commit_free(current);
			current = NULL;
			continue;
		}

		writelogcommit(fp, json, atom, info, ncommits, current);

		// Free previous parent commit and tree (if they exist)
		git_commit_free(current);
		git_tree_free(current_tree);

		ncommits++;
	}

	git_revwalk_free(w);

	fputs("</tbody></table>", fp);
	writefooter(fp);
	fclose(fp);

	fprintf(json, "}}");
	fclose(json);

	writeatomfooter(atom);
	fclose(atom);

	if (maxcommits > 0 && ncommits > maxcommits) {
		if (ncommits - maxcommits == 1)
			fprintf(fp, "<tr><td></td><td colspan=\"5\">1 commit left out...</td></tr>\n");
		else
			fprintf(fp, "<tr><td></td><td colspan=\"5\">%lld commits left out...</td></tr>\n",
			        ncommits - maxcommits);
	}

	return 0;
}
