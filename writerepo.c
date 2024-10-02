#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "parseconfig.h"
#include "writer.h"

#include <limits.h>
#include <string.h>


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
	git_object*     obj = NULL;
	FILE *          fp, *json;
	char            path[PATH_MAX];
	int             i;
	const char*     start;
	char*           confbuffer;

	memset(&info, 0, sizeof(info));
	info.repodir     = repodir;
	info.description = "";
	info.cloneurl    = "";

	info.relpath = 1;
	for (const char* p = repodir + 1; p[1]; p++)
		if (*p == '/')
			info.relpath++;

	start = repodir;
	if (start[0] == '/')
		start++;
	/* use directory name as name */
	strlcpy(info.name, start, sizeof(info.name));
	if (info.name[strlen(info.name) - 1] == '/')
		info.name[strlen(info.name) - 1] = '\0';

	snprintf(info.destdir, sizeof(info.destdir), "%s/%s", destination, info.repodir);
	normalize_path(info.destdir);

	if (columnate)
		printf("%s\t%s\t%s\n", info.name, info.repodir, info.destdir);
	else
		printf("updating '%s' (at %s) -> %s\n", info.name, info.repodir, info.destdir);

	if (mkdirp(info.destdir, 0777) == -1) {
		hprintf(stderr, "error: unable to create destination directory: %w\n", info.destdir);
		exit(100);
	}
	snprintf(path, sizeof(path), "%s/.gitin/files", info.destdir);
	if (mkdirp(path, 0777) == -1) {
		hprintf(stderr, "error: unable to create .gitin/files directory: %w\n", path);
		exit(100);
	}
	snprintf(path, sizeof(path), "%s/.gitin/archive", info.destdir);
	if (mkdirp(path, 0777) == -1) {
		hprintf(stderr, "error: unable to create .gitin/archive directory: %w\n", path);
		exit(100);
	}
	snprintf(path, sizeof(path), "%s/commit", info.destdir);
	if (mkdirp(path, 0777) == -1) {
		hprintf(stderr, "error: unable to create commit directory: %w\n", path);
		exit(100);
	}

	if (git_repository_open_ext(&info.repo, info.repodir, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) < 0) {
		hprintf(stderr, "error: unable to open git repository '%s': %gw\n", info.repodir);
		exit(100);
	}

	/* find HEAD */
	if (!git_revparse_single(&obj, info.repo, "HEAD")) {
		info.head = git_object_id(obj);
	}
	git_object_free(obj);

	snprintf(path, sizeof(path), "%s/%s", repodir, configfile);
	normalize_path(path);

	if ((fp = fopen(path, "r"))) {
		struct config keys[] = {
			{ "description", ConfigString, &info.description },
			{ "url", ConfigString, &info.description },
			{ "cloneurl", ConfigString, &info.description },
			{ 0 },
		};

		if (!(confbuffer = parseconfig(fp, keys)))
			fprintf(stderr, "error: unable to parse config at %s\n", path);

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
	fp   = xfopen("w", "%s/%s", info.destdir, logfile);
	json = xfopen("w", "%s/%s", info.destdir, jsonfile);

	writeheader(fp, &info, 0, info.name, "%y", info.description);
	fprintf(json, "{");
	writerefs(fp, json, &info);

	fputs("<h2>Commits</h2>\n<table id=\"log\"><thead>\n<tr><td><b>Date</b></td>"
	      "<td class=\"expand\"><b>Commit message</b></td>"
	      "<td><b>Author</b></td><td class=\"num\" align=\"right\"><b>Files</b></td>"
	      "<td class=\"num\" align=\"right\"><b>+</b></td>"
	      "<td class=\"num\" align=\"right\"><b>-</b></td></tr>\n</thead><tbody>\n",
	      fp);

	fprintf(json, ",\"commits\":{");
	if (info.head)
		writelog(fp, json, &info);

	fprintf(json, "}}");
	fclose(json);

	fputs("</tbody></table>", fp);
	writefooter(fp);
	fclose(fp);

	if (index)
		writeindexline(index, &info);

	/* cleanup */
	git_repository_free(info.repo);
	free(confbuffer);
	freeheadfiles(&info);
}
