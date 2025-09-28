#include "common.h"
#include "composer.h"
#include "config.h"
#include "getinfo.h"
#include "hprintf.h"
#include "writer.h"

#include <git2/commit.h>
#include <git2/revwalk.h>
#include <limits.h>
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
		hprintf(fp, "<a href=\"../commit/%s.html\">%y</a>", oid, summary);
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
                           git_oid* id) {
	struct commitinfo ci;
	git_commit*       commit;
	git_tree*         tree;
	char              path[PATH_MAX], oid[GIT_OID_SHA1_HEXSIZE + 1];
	int               cachedcommit;

	/* Lookup the current commit */
	if (git_commit_lookup(&commit, info->repo, id)) {
		hprintf(stderr, "error: unable to lookup commit: %gw\n");
		return;
	}

	/* Get the tree for the current commit */
	if (git_commit_tree(&tree, commit)) {
		hprintf(stderr, "error: unable to get tree for commit: %gw\n");
		git_commit_free(commit);
		return;
	}

	git_oid_tostr(oid, sizeof(oid), id);
	snprintf(path, sizeof(path), "commit/%s.html", oid);

	cachedcommit = !force && !access(path, F_OK);
	if (getdiff(&ci, info, commit, cachedcommit) == -1)
		return;

	if (!cachedcommit) {
		FILE* commitfile = efopen("w", "%s", path);
		writecommit(commitfile, info, commit, &ci, index == maxcommits);
		fclose(commitfile);
	}
	if (fp)
		writelogline(fp, commit, &ci);
	if (atom)
		writeatomcommit(atom, commit, NULL);
	if (json)
		writejsoncommit(json, commit, index == 0);

	freediff(&ci);
	git_commit_free(commit);
	git_tree_free(tree);
}

int writelog(FILE* fp, FILE* atom, FILE* json, const struct repoinfo* info,
             struct referenceinfo* refinfo) {
	git_revwalk* w = NULL;
	git_oid      id;
	ssize_t      ncommits = 0, arsize;
	FILE*        arfp;
	const char*  unit;

	emkdirf("commit");
	emkdirf("%s", refinfo->refname);

	if (fp) {
		writeheader(fp, info, 1, 1, info->name, "%s", refinfo->refname);

		fputs("<h2>Archives</h2>", fp);
		fputs("<table><thead>\n<tr><td class=\"expand\">Name</td>"
		      "<td class=\"num\">Size</td></tr>\n</thead><tbody>\n",
		      fp);
	}

	FORMASK(type, archivetypes) {
		arfp   = efopen("w+", "%s/%s.%s", refinfo->refname, refinfo->refname, archiveexts[type]);
		arsize = writearchive(arfp, info, type, refinfo);
		fclose(arfp);
		unit = splitunit(&arsize);
		if (fp)
			fprintf(fp, "<tr><td><a href=\"%s.%s\">%s.%s</a></td><td>%zd%s</td></tr>",
			        refinfo->refname, archiveexts[type], refinfo->refname, archiveexts[type],
			        arsize, unit);
	}

	if (fp) {
		fputs("</tbody></table>", fp);

		writeshortlog(fp, info, refinfo->commit);

		fprintf(fp, "<h2>Commits of %s</h2>", refinfo->refname);

		fprintf(fp, "<table id=\"log\"><thead>\n<tr><td>Date</td>"
		            "<td class=\"expand\">Commit message</td>"
		            "<td>Author</td><td class=\"num\" align=\"right\">Files</td>"
		            "<td class=\"num\" align=\"right\">+</td>"
		            "<td class=\"num\" align=\"right\">-</td></tr>\n</thead><tbody>");
	}
	if (json)
		fprintf(json, "{\"commits\":{");

	if (atom)
		writeatomheader(atom, info);

	/* Create a revwalk to iterate through the commits */
	if (git_revwalk_new(&w, info->repo)) {
		hprintf(stderr, "error: unable to initialize revwalk: %gw\n");
		return -1;
	}
	git_revwalk_push(w, git_commit_id(refinfo->commit));

	/* Iterate through the commits */
	while (!git_revwalk_next(&id, w))
		ncommits++;

	git_revwalk_reset(w);
	git_revwalk_push(w, git_commit_id(refinfo->commit));

	/* Iterate through the commits */
	ssize_t indx = 0;
	while ((maxcommits <= 0 || indx < maxcommits) && !git_revwalk_next(&id, w)) {
		writelogcommit(fp, json, atom, info, indx, &id);
		indx++;

		printprogress(indx, ncommits, "write log:   %-20s", refinfo->refname);
	}
	if (!verbose && !quiet)
		fputc('\n', stdout);

	git_revwalk_free(w);

	if (fp) {
		if (maxcommits > 0 && ncommits > maxcommits) {
			if (ncommits - maxcommits == 1)
				fprintf(fp, "<tr><td></td><td colspan=\"5\">1 commit left out...</td></tr>\n");
			else
				fprintf(fp, "<tr><td></td><td colspan=\"5\">%lld commits left out...</td></tr>\n",
				        ncommits - maxcommits);
		}

		fputs("</tbody></table>", fp);
		writefooter(fp);
	}

	if (json)
		fprintf(json, "}}");

	if (atom)
		writeatomfooter(atom);

	return 0;
}
