#include "common.h"
#include "compat.h"
#include "config.h"
#include "parseconfig.h"
#include "writer.h"

#include <err.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>


void freeheadfiles(struct repoinfo* info) {
	if (info->headfiles != NULL) {
		// Free each string in the list
		for (int i = 0; i < info->headfileslen; i++) {
			free(info->headfiles[i]);
		}
		// Free the list itself
		free(info->headfiles);
		info->headfiles = NULL;
	}

	info->headfileslen   = 0;
	info->headfilesalloc = 0;
}

void writerepo(FILE* index, const char* repodir, const char* destination) {
	struct repoinfo info;

	git_object* obj = NULL;
	FILE*       fp;
	char        path[PATH_MAX];
	int         i;
	const char* start;

	memset(&info, 0, sizeof(info));
	info.repodir = repodir;

	info.relpath = 1;
	for (const char* p = repodir + 1; p[1]; p++)
		if (*p == '/')
			info.relpath++;

	snprintf(info.destdir, sizeof(info.destdir), "%s/%s", destination, info.repodir);
	normalize_path(info.destdir);
	if (mkdirp(info.destdir) == -1) {
		err(1, "cannot create destdir: %s", info.destdir);
		return;
	}

	snprintf(path, sizeof(path), "%s/%s", info.destdir, highlightcache);
	mkdirp(path);
	snprintf(path, sizeof(path), "%s/.gitin/archive", info.destdir);
	mkdirp(path);

	if (git_repository_open_ext(&info.repo, info.repodir, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) < 0) {
		err(1, "cannot open repository");
		return;
	}

	/* find HEAD */
	if (!git_revparse_single(&obj, info.repo, "HEAD"))
		info.head = git_object_id(obj);
	git_object_free(obj);

	start = repodir;
	if (start[0] == '/')
		start++;
	/* use directory name as name */
	strlcpy(info.name, start, sizeof(info.name));
	if (info.name[strlen(info.name) - 1] == '/')
		info.name[strlen(info.name) - 1] = '\0';

	struct configstate state;
	memset(&state, 0, sizeof(state));
	snprintf(path, sizeof(path), "%s/%s", repodir, configfile);
	normalize_path(path);

	if ((fp = fopen(path, "r"))) {
		while (!parseconfig(&state, fp)) {
			if (!strcmp(state.key, "description"))
				strlcpy(info.description, state.value, sizeof(info.description));
			else if (!strcmp(state.key, "url") || !strcmp(state.key, "cloneurl"))
				strlcpy(info.cloneurl, state.value, sizeof(info.cloneurl));
			else
				fprintf(stderr, "warn: ignoring unknown config-key '%s'\n", state.key);
		}
		fclose(fp);
	}

	/* check pinfiles */
	for (i = 0; i < npinfiles && info.pinfileslen < MAXPINS; i++) {
		snprintf(path, sizeof(path), "HEAD:%s", pinfiles[i]);
		if (!git_revparse_single(&obj, info.repo, path) && git_object_type(obj) == GIT_OBJ_BLOB)
			info.pinfiles[info.pinfileslen++] = pinfiles[i];
		git_object_free(obj);
	}

	if (!git_revparse_single(&obj, info.repo, "HEAD:.gitmodules") && git_object_type(obj) == GIT_OBJ_BLOB)
		info.submodules = ".gitmodules";
	git_object_free(obj);

	writefiles(&info);

	/* log for HEAD */
	snprintf(path, sizeof(path), "%s/index.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	fprintf(stderr, "%s\n", path);
	snprintf(path, sizeof(path), "%s/commit", info.destdir);
	mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
	writeheader(fp, &info, 0, info.name, "%y", info.description);
	writerefs(fp, &info);
	fputs("<h2>Commits</h2>\n<table id=\"log\"><thead>\n<tr><td><b>Date</b></td>"
	      "<td class=\"expand\"><b>Commit message</b></td>"
	      "<td><b>Author</b></td><td class=\"num\" align=\"right\"><b>Files</b></td>"
	      "<td class=\"num\" align=\"right\"><b>+</b></td>"
	      "<td class=\"num\" align=\"right\"><b>-</b></td></tr>\n</thead><tbody>\n",
	      fp);

	if (info.head)
		writelog(fp, &info);

	fputs("</tbody></table>", fp);
	writefooter(fp);
	snprintf(path, sizeof(path), "%s/index.html", info.destdir);
	checkfileerror(fp, path, 'w');
	fclose(fp);

	writeindexline(index, &info);
	checkfileerror(index, "index.html", 'w');

	/* cleanup */
	git_repository_free(info.repo);
	freeheadfiles(&info);
}
