#include "gitin.h"

#include <git2/commit.h>
#include <git2/object.h>
#include <git2/refs.h>
#include <git2/repository.h>
#include <git2/revparse.h>
#include <git2/types.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>


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

void addpinfile(struct repoinfo* info, const char* pinfile) {
	char        path[PATH_MAX];
	git_object* obj = NULL;

	snprintf(path, sizeof(path), "HEAD:%s", pinfile);
	if (!git_revparse_single(&obj, info->repo, path)) {
		if (git_object_type(obj) == GIT_OBJECT_BLOB)
			info->pinfiles[info->pinfileslen++] = pinfile;
		git_object_free(obj);
	}
}

void writerepo(struct indexinfo* indexinfo, const char* destination) {
	struct repoinfo info;
	git_object*     obj = NULL;
	FILE *          fp, *json;
	const char*     start;
	char *          confbuffer = NULL, *end;
	char            path[PATH_MAX];

	memset(&info, 0, sizeof(info));
	info.repodir     = indexinfo->repodir;
	info.description = "";
	info.cloneurl    = "";

	info.relpath = 1;
	for (const char* p = info.repodir + 1; p[1]; p++)
		if (*p == '/')
			info.relpath++;

	start = info.repodir;
	if (start[0] == '/')
		start++;
	/* use directory name as name */
	strlcpy(info.name, start, sizeof(info.name));
	if (info.name[strlen(info.name) - 1] == '/')
		info.name[strlen(info.name) - 1] = '\0';

	snprintf(info.destdir, sizeof(info.destdir), "%s/%s", destination, info.repodir);

	if (columnate)
		printf("%s\t%s\t%s\n", info.name, info.repodir, info.destdir);
	else
		printf("updating '%s' (at %s) -> %s\n", info.name, info.repodir, info.destdir);

	emkdirf(0777, "%s", info.destdir);
	emkdirf(0777, "!%s/.cache/archives", info.destdir);
	emkdirf(0777, "!%s/.cache/files", info.destdir);
	emkdirf(0777, "!%s/.cache/diffs", info.destdir);
	emkdirf(0777, "!%s/.cache/pandoc", info.destdir);
	emkdirf(0777, "!%s/.cache/blobs", info.destdir);
	emkdirf(0777, "%s/archive", info.destdir);
	emkdirf(0777, "%s/commit", info.destdir);

	if (git_repository_open_ext(&info.repo, info.repodir, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) <
	    0) {
		hprintf(stderr, "error: unable to open git repository '%s': %gw\n", info.repodir);
		exit(100);
	}

	if ((fp = efopen("!.r", "%s/%s", info.repodir, configfile))) {
		struct configitem keys[] = {
			{ "description", ConfigString, &info.description },
			{ "url", ConfigString, &info.description },
			{ "cloneurl", ConfigString, &info.description },
			//			{ "revision", ConfigString, &info.revision },
			{ 0 },
		};

		confbuffer = parseconfig(fp, keys);
		fclose(fp);
	}

	/* check pinfiles */
	for (int i = 0; pinfiles[i] && info.pinfileslen < MAXPINS; i++) {
		addpinfile(&info, pinfiles[i]);
	}

	if (extrapinfiles) {
		start = extrapinfiles;

		// Loop through the string, finding each space and treating it as a delimiter
		while (info.pinfileslen < MAXPINS && (end = strchr(start, ' ')) != NULL) {
			*end = '\0';    // Replace the space with a null terminator to isolate the token
			addpinfile(&info, start);
			start = end + 1;
		}

		addpinfile(&info, start);
	}

	if (!git_revparse_single(&obj, info.repo, "HEAD:.gitmodules")) {
		if (git_object_type(obj) == GIT_OBJECT_BLOB)
			info.submodules = ".gitmodules";
		git_object_free(obj);
	}
	//	writefiles(&info);


	git_repository_head(&info.head, info.repo);

	fp   = efopen("w", "%s/index.html", info.destdir);
	json = efopen("w", "%s/index.json", info.destdir);
	writeheader(fp, &info, 0, info.name, "%y", info.description);
	writerefs(fp, json, &info);
	writefooter(fp);
	fclose(fp);
	fclose(json);

	snprintf(path, sizeof(path), "%s/file/HEAD", info.destdir);
	symlink(git_reference_shorthand(info.head), path);

	// expect it to be malloced
	indexinfo->name        = strdup(info.name);
	indexinfo->description = strdup(info.description);

	/* cleanup */
	git_reference_free(info.head);
	git_repository_free(info.repo);
	free(confbuffer);
	freeheadfiles(&info);
}
