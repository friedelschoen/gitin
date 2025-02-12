#include "common.h"
#include "getinfo.h"
#include "hprintf.h"

#include <git2/commit.h>
#include <string.h>


/* Function to dump the commitstats struct into a file */
static void dumpdiff(FILE* fp, const struct commitinfo* stats) {
	/* Write commitstats fields */
	fwrite(&stats->addcount, sizeof(size_t), 1, fp);
	fwrite(&stats->delcount, sizeof(size_t), 1, fp);

	/* Write the number of deltas */
	fwrite(&stats->ndeltas, sizeof(size_t), 1, fp);

	/* Write each delta (if any) */
	for (size_t i = 0; i < stats->ndeltas; i++) {
		fwrite(&stats->deltas[i].addcount, sizeof(size_t), 1, fp);
		fwrite(&stats->deltas[i].delcount, sizeof(size_t), 1, fp);
	}
}

/* Function to parse the commitstats struct from a file */
static void loaddiff(FILE* fp, struct commitinfo* stats) {
	/* Read commitstats fields */
	fread(&stats->addcount, sizeof(size_t), 1, fp);
	fread(&stats->delcount, sizeof(size_t), 1, fp);

	/* Read the number of deltas */
	fread(&stats->ndeltas, sizeof(size_t), 1, fp);

	/* Allocate memory for the deltas array */
	stats->deltas = NULL; /* No deltas to store */
	if (stats->ndeltas > 0) {
		stats->deltas = malloc(stats->ndeltas * sizeof(struct deltainfo));
		if (!stats->deltas) {
			fprintf(stderr, "Memory allocation failed\n");
			exit(100);
		}

		/* Read each delta */
		for (size_t i = 0; i < stats->ndeltas; i++) {
			stats->deltas[i].patch = NULL;
			fread(&stats->deltas[i].addcount, sizeof(size_t), 1, fp);
			fread(&stats->deltas[i].delcount, sizeof(size_t), 1, fp);
		}
	}
}

int getdiff(struct commitinfo* ci, const struct repoinfo* info, git_commit* commit, int docache) {
	git_diff_options      opts;
	git_diff_find_options fopts;
	const git_diff_delta* delta;
	const git_diff_hunk*  hunk;
	const git_diff_line*  line;
	git_patch*            patch = NULL;
	size_t                nhunks, nhunklines;
	git_diff*             diff = NULL;
	git_commit*           parent;
	git_tree *            commit_tree, *parent_tree;
	FILE*                 fp;
	char                  oid[GIT_OID_SHA1_HEXSIZE + 1];

	emkdirf("!%s/.cache/diffs", info->destdir);

	git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));

	if (docache && (fp = efopen("!.r", "%s/.cache/diffs/%s", info->destdir, oid))) {
		loaddiff(fp, ci);
		fclose(fp);
		return 0;
	}

	if (git_commit_tree(&commit_tree, commit)) {
		hprintf(stderr, "error: unable to fetch tree: %gw");
		return -1;
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

	git_diff_options_init(&opts, GIT_DIFF_OPTIONS_VERSION);
	git_diff_find_options_init(&fopts, GIT_DIFF_FIND_OPTIONS_VERSION);
	opts.flags |=
	    GIT_DIFF_DISABLE_PATHSPEC_MATCH | GIT_DIFF_IGNORE_SUBMODULES | GIT_DIFF_INCLUDE_TYPECHANGE;
	fopts.flags |= GIT_DIFF_FIND_RENAMES | GIT_DIFF_FIND_COPIES | GIT_DIFF_FIND_EXACT_MATCH_ONLY;

	memset(ci, 0, sizeof(*ci));

	if (git_diff_tree_to_tree(&diff, info->repo, parent_tree, commit_tree, &opts)) {
		hprintf(stderr, "error: unable to generate diff: %gw\n");
		goto err;
	}

	if (git_diff_find_similar(diff, &fopts)) {
		hprintf(stderr, "error: unable to find similar changes: %gw\n");
		goto err;
	}

	ci->ndeltas = git_diff_num_deltas(diff);
	if (ci->ndeltas && !(ci->deltas = calloc(ci->ndeltas, sizeof(struct deltainfo)))) {
		hprintf(stderr, "error: unable to allocate memory for deltas: %w\n");
		exit(100); /* Fatal error */
	}

	for (size_t i = 0; i < ci->ndeltas; i++) {
		if (git_patch_from_diff(&patch, diff, i)) {
			hprintf(stderr, "error: unable to create patch from diff: %gw\n");
			goto err;
		}

		ci->deltas[i].patch    = patch;
		ci->deltas[i].addcount = 0;
		ci->deltas[i].delcount = 0;

		delta = git_patch_get_delta(patch);

		if (delta->flags & GIT_DIFF_FLAG_BINARY)
			continue;

		nhunks = git_patch_num_hunks(patch);
		for (size_t j = 0; j < nhunks; j++) {
			if (git_patch_get_hunk(&hunk, &nhunklines, patch, j)) {
				hprintf(stderr, "error: unable to get hunk: %gw\n");
				break;
			}
			for (size_t k = 0;; k++) {
				if (git_patch_get_line_in_hunk(&line, patch, j, k))
					break;
				if (line->old_lineno == -1) {
					ci->deltas[i].addcount++;
					ci->addcount++;
				} else if (line->new_lineno == -1) {
					ci->deltas[i].delcount++;
					ci->delcount++;
				}
			}
		}
	}

	git_diff_free(diff);
	git_commit_free(parent);
	git_tree_free(commit_tree);
	git_tree_free(parent_tree);

	if ((fp = efopen("!.w", "%s/.cache/diffs/%s", info->destdir, oid))) {
		dumpdiff(fp, ci);
		fclose(fp);
	}

	return 0;

err:
	git_diff_free(diff);
	git_commit_free(parent);
	git_tree_free(commit_tree);
	git_tree_free(parent_tree);

	freediff(ci);
	return -1;
}

void freediff(struct commitinfo* ci) {
	size_t i;

	if (!ci)
		return;

	if (ci->deltas) {
		for (i = 0; i < ci->ndeltas; i++) {
			git_patch_free(ci->deltas[i].patch);
		}
		free(ci->deltas);
	}

	memset(ci, 0, sizeof(*ci)); /* Reset structure */
}
