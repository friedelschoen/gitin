#include "commit.h"
#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "writer.h"

#include <err.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/oid.h>
#include <git2/types.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>


static void writecommit(FILE* fp, int relpath, const git_commit* commit) {
	char                 oid[GIT_OID_HEXSZ + 1], parentoid[GIT_OID_HEXSZ + 1];
	const git_signature* author = git_commit_author(commit);
	const char*          msg    = git_commit_message(commit);

	git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));
	git_oid_tostr(parentoid, sizeof(parentoid), git_commit_parent_id(commit, 0));

	hprintf(fp, "<b>commit</b> <a href=\"%rcommit/%s.html\">%s</a>\n", relpath, oid, oid);

	if (*parentoid)
		hprintf(fp, "<b>parent</b> <a href=\"%rcommit/%s.html\">%s</a>\n", relpath, parentoid, parentoid);

	if (author)
		hprintf(fp, "<b>Author:</b> %y &lt;<a href=\"mailto:%y\">%y</a>&gt;\n<b>Date:</b>   %T\n", author->name,
		        author->email, author->email, &author->when);

	if (msg)
		hprintf(fp, "\n%y\n", msg);
}

static int hasheadfile(const struct repoinfo* info, const char* filename) {
	for (int i = 0; i < info->headfileslen; i++) {
		if (strcmp(info->headfiles[i], filename) == 0) {
			return 1;
		}
	}
	return 0;
}

static void writediff(FILE* fp, const struct repoinfo* info, int relpath, git_commit* commit, struct commitstats* ci) {
	const git_diff_delta* delta;
	const git_diff_hunk*  hunk;
	const git_diff_line*  line;
	git_patch*            patch;
	size_t                nhunks, nhunklines, changed, add, del, total, i, j, k;
	char                  linestr[80];
	int                   c;

	writecommit(fp, relpath, commit);

	if (!ci->deltas)
		return;

	if (ci->filecount > 1000 || ci->ndeltas > 1000 || ci->addcount > 100000 || ci->delcount > 100000) {
		fputs("Diff is too large, output suppressed.\n", fp);
		return;
	}

	/* diff stat */
	fputs("<b>Diffstat:</b>\n<table>", fp);
	for (i = 0; i < ci->ndeltas; i++) {
		delta = git_patch_get_delta(ci->deltas[i]->patch);

		switch (delta->status) {
			case GIT_DELTA_ADDED:
				c = 'A';
				break;
			case GIT_DELTA_COPIED:
				c = 'C';
				break;
			case GIT_DELTA_DELETED:
				c = 'D';
				break;
			case GIT_DELTA_MODIFIED:
				c = 'M';
				break;
			case GIT_DELTA_RENAMED:
				c = 'R';
				break;
			case GIT_DELTA_TYPECHANGE:
				c = 'T';
				break;
			default:
				c = ' ';
				break;
		}
		if (c == ' ')
			fprintf(fp, "<tr><td>%c", c);
		else
			fprintf(fp, "<tr><td class=\"%c\">%c", c, c);

		hprintf(fp, "</td><td><a href=\"#h%zu\">%y", i, delta->old_file.path);
		if (strcmp(delta->old_file.path, delta->new_file.path))
			hprintf(fp, " -&gt; \n%y", delta->new_file.path);

		add     = ci->deltas[i]->addcount;
		del     = ci->deltas[i]->delcount;
		changed = add + del;
		total   = sizeof(linestr) - 2;
		if (changed > total) {
			if (add)
				add = ((float) total / changed * add) + 1;
			if (del)
				del = ((float) total / changed * del) + 1;
		}
		memset(&linestr, '+', add);
		memset(&linestr[add], '-', del);

		fprintf(fp, "</a></td><td class=\"num\">%zu</td><td class=\"expand\"><span class=\"i\">",
		        ci->deltas[i]->addcount + ci->deltas[i]->delcount);
		fwrite(&linestr, 1, add, fp);
		fputs("</span><span class=\"d\">", fp);
		fwrite(&linestr[add], 1, del, fp);
		fputs("</span></td></tr>\n", fp);
	}
	fprintf(fp, "</table></pre><pre>%zu file%s changed, %zu insertion%s(+), %zu deletion%s(-)\n", ci->filecount,
	        ci->filecount == 1 ? "" : "s", ci->addcount, ci->addcount == 1 ? "" : "s", ci->delcount,
	        ci->delcount == 1 ? "" : "s");

	fputs("<hr/>", fp);

	for (i = 0; i < ci->ndeltas; i++) {
		patch = ci->deltas[i]->patch;
		delta = git_patch_get_delta(patch);

		if (hasheadfile(info, delta->old_file.path))
			hprintf(fp, "<b>diff --git a/<a id=\"h%zu\" href=\"%rfile/%h.html\">%y</a> ", i, relpath,
			        delta->old_file.path, delta->old_file.path);
		else
			hprintf(fp, "<b>diff --git a/%y ", delta->old_file.path);

		if (hasheadfile(info, delta->new_file.path))
			hprintf(fp, "b/<a href=\"%rfile/%h.html\">%y</a></b>\n", relpath, delta->new_file.path,
			        delta->new_file.path);
		else
			hprintf(fp, "b/%y</b>\n", delta->new_file.path);

		if (delta->flags & GIT_DIFF_FLAG_BINARY) {
			fputs("Binary files differ.\n", fp);
			continue;
		}

		nhunks = git_patch_num_hunks(patch);
		for (j = 0; j < nhunks; j++) {
			if (git_patch_get_hunk(&hunk, &nhunklines, patch, j))
				break;

			hprintf(fp, "<a href=\"#h%zu-%zu\" id=\"h%zu-%zu\" class=\"h\">%y</a>\n", i, j, i, j, hunk->header);

			for (k = 0;; k++) {
				if (git_patch_get_line_in_hunk(&line, patch, j, k))
					break;
				if (line->old_lineno == -1)
					fprintf(fp, "<a href=\"#h%zu-%zu-%zu\" id=\"h%zu-%zu-%zu\" class=\"i\">+", i, j, k, i, j, k);
				else if (line->new_lineno == -1)
					fprintf(fp, "<a href=\"#h%zu-%zu-%zu\" id=\"h%zu-%zu-%zu\" class=\"d\">-", i, j, k, i, j, k);
				else
					putc(' ', fp);
				xmlencodeline(fp, line->content, line->content_len);
				putc('\n', fp);
				if (line->old_lineno == -1 || line->new_lineno == -1)
					fputs("</a>", fp);
			}
		}
	}
}

static void writelogline(FILE* fp, int relpath, git_commit* commit, const struct commitstats* ci) {
	char                 oid[GIT_OID_HEXSZ + 1];
	const git_signature* author  = git_commit_author(commit);
	const char*          summary = git_commit_summary(commit);

	git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));

	fputs("<tr><td>", fp);
	if (author)
		hprintf(fp, "%t", &author->when);
	fputs("</td><td>", fp);
	if (summary) {
		hprintf(fp, "<a href=\"%rcommit/%s.html\">%y</a>", relpath, oid, summary);
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

int writelog(FILE* fp, const struct repoinfo* info) {
	git_commit*        commit = NULL;
	git_revwalk*       w      = NULL;
	git_oid            id;
	char               path[PATH_MAX], oidstr[GIT_OID_HEXSZ + 1];
	FILE*              fpfile;
	size_t             remcommits = 0;
	const char*        summary;
	struct commitstats ci;

	// Create a revwalk to iterate through the commits
	if (git_revwalk_new(&w, info->repo)) {
		fprintf(stderr, "Error initializing revwalk\n");
		return -1;
	}
	git_revwalk_push_head(w);

	// Iterate through the commits
	while (!git_revwalk_next(&id, w)) {
		// Lookup the commit object
		if (git_commit_lookup(&commit, info->repo, &id)) {
			fprintf(stderr, "Error looking up commit\n");
			continue;
		}

		if (commitinfo_getstats(&ci, commit, info->repo) == -1)
			continue;

		git_oid_tostr(oidstr, sizeof(oidstr), &id);
		snprintf(path, sizeof(path), "%s/commit/%s.html", info->destdir, oidstr);
		normalize_path(path);

		// Skip commits that are already written
		if (!nlogcommits) {
			remcommits++;
		}

		summary = git_commit_summary(commit);

		// Write the commit's diff to a file
		if (!(fpfile = fopen(path, "w"))) {
			err(1, "fopen: '%s'", path);
		}
		fprintf(stderr, "%s\n", path);
		writeheader(fpfile, info, 1, info->name, "%y", summary);
		fputs("<pre>", fpfile);
		(void) writediff;
		writediff(fpfile, info, 1, commit, &ci);
		fputs("</pre>\n", fpfile);
		writefooter(fpfile);
		checkfileerror(fpfile, path, 'w');
		fclose(fpfile);

		// Write the log line
		if (nlogcommits != 0) {
			(void) writelogline;
			writelogline(fp, 0, commit, &ci);
			if (nlogcommits > 0)
				nlogcommits--;
		}

		commitinfo_free(&ci);
		git_commit_free(commit);
	}

	git_revwalk_free(w);

	// Handle remaining commits
	if (nlogcommits == 0 && remcommits != 0) {
		fprintf(fp,
		        "<tr><td></td><td colspan=\"5\">"
		        "%zu more commits remaining, fetch the repository"
		        "</td></tr>\n",
		        remcommits);
	}

	return 0;
}
