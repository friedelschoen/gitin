#include "gitin.h"

#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/refs.h>
#include <git2/revwalk.h>
#include <git2/types.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

static void writelogline(FILE* fp, git_commit* commit, const struct commitinfo* ci) {
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
	fprintf(fp, "%zu", ci->ndeltas);
	fputs("</td><td class=\"num\" align=\"right\">", fp);
	fprintf(fp, "+%zu", ci->addcount);
	fputs("</td><td class=\"num\" align=\"right\">", fp);
	fprintf(fp, "-%zu", ci->delcount);
	fputs("</td></tr>\n", fp);
}

static void writelogcommit(FILE* fp, FILE* json, FILE* atom, const struct repoinfo* info, int index,
                           git_oid* id, const char* refname) {

	struct commitinfo ci;
	git_commit*       commit;
	git_tree*         tree;
	char              path[PATH_MAX], oid[GIT_OID_SHA1_HEXSIZE + 1];
	int               cachedcommit;

	// Lookup the current commit
	if (git_commit_lookup(&commit, info->repo, id)) {
		hprintf(stderr, "error: unable to lookup commit: %gw\n");
		return;
	}

	// Get the tree for the current commit
	if (git_commit_tree(&tree, commit)) {
		hprintf(stderr, "error: unable to get tree for commit: %gw\n");
		git_commit_free(commit);
		return;
	}

	git_oid_tostr(oid, sizeof(oid), id);
	snprintf(path, sizeof(path), "%s/commit/%s.html", info->destdir, oid);

	cachedcommit = !force && !access(path, F_OK);
	if (getdiff(&ci, info, commit, cachedcommit) == -1)
		return;

	if (!cachedcommit)
		writecommitfile(info, commit, &ci, index == maxcommits, refname);

	writelogline(fp, commit, &ci);
	writecommitatom(atom, commit, NULL);
	writejsoncommit(json, commit, index == 0);

	freediff(&ci);
	git_commit_free(commit);
	git_tree_free(tree);
}

int writelog(const struct repoinfo* info, git_reference* ref, git_commit* head) {
	git_revwalk* w = NULL;
	git_oid      id;
	ssize_t      ncommits = 0, arsize;
	FILE *       fp, *atom, *json;
	const char * unit, *refname;

	refname = git_reference_shorthand(ref);

	/* log for HEAD */
	fp   = efopen("w", "%s/%s/index.html", info->destdir, refname);
	json = efopen("w", "%s/%s/branch.json", info->destdir, refname);
	atom = efopen("w", "%s/%s/atom.xml", info->destdir, refname);

	writeheader(fp, info, 1, info->name, "%s", refname);
	fprintf(json, "{");

	writerefs(fp, info, 1, ref);
	fputs("<hr>", fp);

	fputs("<h2>Archives</h2>", fp);
	fputs("<table><thead>\n<tr><td class=\"expand\">Name</td>"
	      "<td class=\"num\">Size</td></tr>\n</thead><tbody>\n",
	      fp);

	FORMASK(type, archivetypes) {
		arsize = writearchive(info, type, ref, head);
		unit   = splitunit(&arsize);
		fprintf(fp, "<tr><td><a href=\"archive/%s.tar.gz\">%s.%s</a></td><td>%zd%s</td></tr>",
		        refname, refname, archiveexts[type], arsize, unit);
	}
	fputs("</tbody></table>", fp);

	writeshortlog(fp, info, head);

	fprintf(fp, "<h2>Commits of %s</h2>", refname);

	fprintf(fp, "<table id=\"log\"><thead>\n<tr><td>Date</td>"
	            "<td class=\"expand\">Commit message</td>"
	            "<td>Author</td><td class=\"num\" align=\"right\">Files</td>"
	            "<td class=\"num\" align=\"right\">+</td>"
	            "<td class=\"num\" align=\"right\">-</td></tr>\n</thead><tbody>");

	fprintf(json, ",\"commits\":{");

	writeatomheader(atom, info);

	// Create a revwalk to iterate through the commits
	if (git_revwalk_new(&w, info->repo)) {
		hprintf(stderr, "error: unable to initialize revwalk: %gw\n");
		return -1;
	}
	git_revwalk_push(w, git_commit_id(head));

	// Iterate through the commits
	while (!git_revwalk_next(&id, w))
		ncommits++;

	git_revwalk_reset(w);
	git_revwalk_push(w, git_commit_id(head));

	// Iterate through the commits
	ssize_t indx = 0;
	while ((maxcommits <= 0 || indx < maxcommits) && !git_revwalk_next(&id, w)) {
		writelogcommit(fp, json, atom, info, indx, &id, refname);
		indx++;

		printprogress(indx, ncommits, "write log:   %-20s", refname);
	}
	if (!verbose)
		fputc('\n', stdout);

	git_revwalk_free(w);

	if (maxcommits > 0 && ncommits > maxcommits) {
		if (ncommits - maxcommits == 1)
			fprintf(fp, "<tr><td></td><td colspan=\"5\">1 commit left out...</td></tr>\n");
		else
			fprintf(fp, "<tr><td></td><td colspan=\"5\">%lld commits left out...</td></tr>\n",
			        ncommits - maxcommits);
	}

	fputs("</tbody></table>", fp);
	writefooter(fp);
	fclose(fp);

	fprintf(json, "}}");
	fclose(json);

	writeatomfooter(atom);
	fclose(atom);

	return 0;
}
