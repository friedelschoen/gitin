#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "writer.h"

#include <git2/commit.h>
#include <git2/revwalk.h>
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

static int getdiff(struct commitstats* ci, git_repository* repo, git_tree* commit_tree,
                   git_tree* parent_tree) {
	struct deltainfo*     di;
	git_diff_options      opts;
	git_diff_find_options fopts;
	const git_diff_delta* delta;
	const git_diff_hunk*  hunk;
	const git_diff_line*  line;
	git_patch*            patch = NULL;
	size_t                ndeltas, nhunks, nhunklines;
	size_t                i, j, k;
	git_diff*             diff = NULL;

	git_diff_options_init(&opts, GIT_DIFF_OPTIONS_VERSION);
	git_diff_find_options_init(&fopts, GIT_DIFF_FIND_OPTIONS_VERSION);

	opts.flags |=
	    GIT_DIFF_DISABLE_PATHSPEC_MATCH | GIT_DIFF_IGNORE_SUBMODULES | GIT_DIFF_INCLUDE_TYPECHANGE;
	fopts.flags |= GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES | GIT_DIFF_FIND_EXACT_MATCH_ONLY;

	memset(ci, 0, sizeof(*ci));

	if (git_diff_tree_to_tree(&diff, repo, parent_tree, commit_tree, &opts)) {
		hprintf(stderr, "error: unable to generate diff: %gw\n");
		goto err;
	}

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
	return 0;

err:
	git_diff_free(diff);
	freestats(ci);
	return -1;
}

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
	git_commit*        parent;
	git_tree *         commit_tree, *parent_tree = NULL;

	memset(&ci, 0, sizeof(ci));

	if (git_commit_tree(&commit_tree, commit)) {
		hprintf(stderr, "error: unable to fetch tree: %gw");
		return;
	}

	if (git_commit_parent(&parent, commit, 0)) {
		parent      = NULL;
		parent_tree = NULL;
	} else if (git_commit_tree(&parent_tree, parent)) {
		hprintf(stderr, "warn: unable to fetch parent-tree: %gw");
		git_commit_free(parent);
		parent      = NULL;
		parent_tree = NULL;
	}

	if (getdiff(&ci, info->repo, commit_tree, parent_tree) == -1)
		return;

	writecommitfile(info, commit, &ci, index == maxcommits);

	writelogline(fp, commit, &ci);
	writecommitatom(atom, commit, NULL);
	writejsoncommit(json, commit, index == 0);

	freestats(&ci);
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
