#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "parseconfig.h"
#include "writer.h"

#include <limits.h>
#include <string.h>


int writeindexline(FILE* fp, const struct repoinfo* info) {
	git_commit*          commit = NULL;
	const git_signature* author;
	git_revwalk*         w = NULL;
	git_oid              id;
	int                  ret = 0;

	git_revwalk_new(&w, info->repo);
	if (git_revwalk_push_head(w)) {
		hprintf(stderr, "error: unable to push head: %gw\n");
		return -1;
	}
	if (git_revwalk_next(&id, w) || git_commit_lookup(&commit, info->repo, &id)) {
		hprintf(stderr, "error: unable to lookup commit: %gw\n");
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

static void writecategory(FILE* index, const char* name, int len) {
	char               category[PATH_MAX], configpath[PATH_MAX];
	char*              description = "";
	struct configstate state       = { 0 };
	FILE*              fp;

	memcpy(category, name, len);
	category[len] = '\0';

	snprintf(configpath, sizeof(configpath), "%s/%s", category, configfile);
	if ((fp = fopen(configpath, "r"))) {
		while (!parseconfig(&state, fp)) {
			if (!strcmp(state.key, "description"))
				description = state.value;
			else
				hprintf(stderr, "warn: ignoring unknown config-key '%s'\n", state.key);
		}
		fclose(fp);
	}

	hprintf(index, "<tr class=\"category\"><td>%y</td><td>%y</td><td></td></tr>\n", category, description);

	parseconfig_free(&state);
}

static int sortpath(const void* leftp, const void* rightp) {
	const char* left  = *(const char**) leftp;
	const char* right = *(const char**) rightp;

	return strcmp(left, right);
}

void writeindex(const char* destdir, char** repos, int nrepos) {
	FILE* index;
	char  path[PATH_MAX];

	strlcpy(path, destdir, sizeof(path));
	if (mkdirp(path, 0777)) {
		hprintf(stderr, "error: unable to create directory %s: %w\n", path);
		exit(100);
	}

	index = xfopen("w+", "%s/index.html", destdir);
	writeheader(index, NULL, 0, sitename, "%y", sitedescription);
	fputs("<table id=\"index\"><thead>\n"
	      "<tr><td><b>Name</b></td><td class=\"expand\"><b>Description</b></td><td><b>Last changes</b></td></tr>"
	      "</thead><tbody>\n",
	      index);

	qsort(repos, nrepos, sizeof(char*), sortpath);

	const char* category    = NULL;
	int         categorylen = 0, curlen;
	for (int i = 0; i < nrepos; i++) {
		curlen = strchr(repos[i], '/') ? strrchr(repos[i], '/') - repos[i] : 0;
		if (!category || curlen != categorylen || strncmp(category, repos[i], categorylen) != 0) {
			category    = repos[i];
			categorylen = curlen;
			writecategory(index, category, categorylen);
		}

		writerepo(index, repos[i], destdir);
	}
	fputs("</tbody>\n</table>", index);
	writefooter(index);
	fclose(index);
}
