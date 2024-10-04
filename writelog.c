#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "writer.h"

#include <limits.h>
#include <string.h>
#include <unistd.h>


void freestats(struct commitstats* ci) {
	size_t i;

	if (!ci)
		return;

	if (ci->deltas) {
		for (i = 0; i < ci->ndeltas; i++) {
			if (!ci->deltas[i])
				continue;
			git_patch_free(ci->deltas[i]->patch);
			free(ci->deltas[i]);
		}
		free(ci->deltas);
	}

	memset(ci, 0, sizeof(*ci));    // Reset structure
}

static int getstats(struct commitstats* ci, git_commit* commit, git_repository* repo) {
	struct deltainfo*     di;
	git_diff_options      opts;
	git_diff_find_options fopts;
	const git_diff_delta* delta;
	const git_diff_hunk*  hunk;
	const git_diff_line*  line;
	git_patch*            patch = NULL;
	size_t                ndeltas, nhunks, nhunklines;
	size_t                i, j, k;

	git_commit* parent      = NULL;
	git_tree*   commit_tree = NULL;
	git_tree*   parent_tree = NULL;
	git_diff*   diff        = NULL;

	memset(ci, 0, sizeof(*ci));

	if (git_tree_lookup(&commit_tree, repo, git_commit_tree_id(commit))) {
		hprintf(stderr, "error: unable to look up commit tree: %gw\n");
		goto err;
	}

	if (!git_commit_parent(&parent, commit, 0)) {
		if (git_tree_lookup(&parent_tree, repo, git_commit_tree_id(parent))) {
			parent      = NULL;
			parent_tree = NULL;
		}
	}

	git_diff_options_init(&opts, GIT_DIFF_OPTIONS_VERSION);
	opts.flags |=
	    GIT_DIFF_DISABLE_PATHSPEC_MATCH | GIT_DIFF_IGNORE_SUBMODULES | GIT_DIFF_INCLUDE_TYPECHANGE;
	if (git_diff_tree_to_tree(&diff, repo, parent_tree, commit_tree, &opts)) {
		hprintf(stderr, "error: unable to generate diff: %gw\n");
		goto err;
	}

	git_diff_find_options_init(&fopts, GIT_DIFF_FIND_OPTIONS_VERSION);
	fopts.flags |= GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES | GIT_DIFF_FIND_EXACT_MATCH_ONLY;
	if (git_diff_find_similar(diff, &fopts)) {
		hprintf(stderr, "error: unable to find similar changes: %gw\n");
		goto err;
	}

	ndeltas = git_diff_num_deltas(diff);
	if (ndeltas && !(ci->deltas = calloc(ndeltas, sizeof(struct deltainfo*)))) {
		hprintf(stderr, "error: unable to allocate memory for deltas: %w\n");
		exit(100);    // Fatal error
	}

	for (i = 0; i < ndeltas; i++) {
		if (git_patch_from_diff(&patch, diff, i)) {
			hprintf(stderr, "error: unable to create patch from diff: %gw\n");
			goto err;
		}

		if (!(di = calloc(1, sizeof(struct deltainfo)))) {
			hprintf(stderr, "error: unable to allocate memory for deltainfo: %w\n");
			exit(100);    // Fatal error
		}
		di->patch     = patch;
		ci->deltas[i] = di;

		delta = git_patch_get_delta(patch);

		if (delta->flags & GIT_DIFF_FLAG_BINARY)
			continue;

		nhunks = git_patch_num_hunks(patch);
		for (j = 0; j < nhunks; j++) {
			if (git_patch_get_hunk(&hunk, &nhunklines, patch, j)) {
				hprintf(stderr, "error: unable to get hunk: %gw\n");
				break;
			}
			for (k = 0;; k++) {
				if (git_patch_get_line_in_hunk(&line, patch, j, k))
					break;
				if (line->old_lineno == -1) {
					di->addcount++;
					ci->addcount++;
				} else if (line->new_lineno == -1) {
					di->delcount++;
					ci->delcount++;
				}
			}
		}
	}
	ci->ndeltas   = i;
	ci->filecount = i;

	git_diff_free(diff);
	git_tree_free(commit_tree);
	git_tree_free(parent_tree);
	git_commit_free(parent);

	return 0;

err:
	git_diff_free(diff);
	git_tree_free(commit_tree);
	git_tree_free(parent_tree);
	git_commit_free(parent);
	freestats(ci);
	return -1;
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

int writelog(FILE* fp, FILE* json, const struct repoinfo* info) {
	git_commit*        commit = NULL;
	git_revwalk*       w      = NULL;
	git_oid            id;
	char               path[PATH_MAX], oidstr[GIT_OID_HEXSZ + 1];
	FILE*              fpfile;
	ssize_t            ncommits = 0;
	const char*        summary;
	struct commitstats ci;
	FILE*              atom;
	int                first;

	atom = xfopen("w", "%s/%s", info->destdir, commitatomfile);
	writeatomheader(atom, info);

	// Create a revwalk to iterate through the commits
	if (git_revwalk_new(&w, info->repo)) {
		hprintf(stderr, "error: unable to initialize revwalk: %gw\n");
		return -1;
	}
	git_revwalk_push_head(w);

	// Iterate through the commits
	while (!git_revwalk_next(&id, w)) {
		first = ncommits == 0;
		ncommits++;
		if (maxcommits > 0 && ncommits > maxcommits)
			continue;

		// Lookup the commit object
		if (git_commit_lookup(&commit, info->repo, &id)) {
			hprintf(stderr, "error: unable to lookup commit: %gw\n");
			continue;
		}

		git_oid_tostr(oidstr, sizeof(oidstr), &id);

		writecommitatom(atom, commit, NULL);
		if (!first) {
			fprintf(json, ",");
		}
		fprintf(json, "\"%s\":", oidstr);
		writejsoncommit(json, commit);

		if (getstats(&ci, commit, info->repo) == -1)
			continue;

		snprintf(path, sizeof(path), "%s/commit/%s.html", info->destdir, oidstr);

		// if it does not exist yet
		if (force || access(path, F_OK)) {
			summary = git_commit_summary(commit);

			fpfile = xfopen("w", "%s", path);
			writeheader(fpfile, info, 1, info->name, "%y", summary);
			fputs("<pre>", fpfile);
			writediff(fpfile, info, commit, &ci, ncommits != maxcommits);
			fputs("</pre>\n", fpfile);
			writefooter(fpfile);
			fclose(fpfile);
		}

		writelogline(fp, 0, commit, &ci);

		freestats(&ci);
		git_commit_free(commit);
	}

	git_revwalk_free(w);

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
