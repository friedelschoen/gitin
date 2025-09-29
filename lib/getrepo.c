#include "common.h"
#include "config.h"
#include "getinfo.h"
#include "hprintf.h"

#include <git2/repository.h>
#include <git2/revparse.h>
#include <git2/types.h>
#include <limits.h>
#include <string.h>


static void freeheadfiles(struct repoinfo* info) {
	if (info->headfiles != NULL) {
		/* Free each string in the list */
		for (int i = 0; i < info->headfileslen; i++) {
			free(info->headfiles[i]);
		}
		/* Free the list itself */
		free(info->headfiles);
		info->headfiles = NULL;
	}

	info->headfileslen   = 0;
	info->headfilesalloc = 0;
}

static void addpinfile(struct repoinfo* info, const char* pinfile) {
	char        path[PATH_MAX];
	git_object* obj = NULL;

	snprintf(path, sizeof(path), "HEAD:%s", pinfile);
	if (!git_revparse_single(&obj, info->repo, path)) {
		if (git_object_type(obj) == GIT_OBJECT_BLOB)
			info->pinfiles[info->pinfileslen++] = pinfile;
		git_object_free(obj);
	}
}

void getrepo(struct repoinfo* info, const char* repodir) {
	git_object*    obj = NULL;
	FILE*          fp;
	const char *   start, *reqbranchname = NULL;
	char*          end;
	git_reference* branch = NULL;

	memset(info, 0, sizeof(*info));
	info->repodir     = repodir;
	info->description = "";
	info->cloneurl    = "";

	info->relpath = 1;
	for (const char* p = info->repodir + 1; p[1]; p++)
		if (*p == '/')
			info->relpath++;

	start = info->repodir;
	if (start[0] == '/')
		start++;
	/* use directory name as name */
	strlcpy(info->name, start, sizeof(info->name));
	if (info->name[strlen(info->name) - 1] == '/')
		info->name[strlen(info->name) - 1] = '\0';

	if (git_repository_open_ext(&info->repo, info->repodir, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) <
	    0) {
		hprintf(stderr, "error: unable to open git repository '%s': %gw\n", info->repodir);
		exit(100);
	}

	if ((fp = efopen("!.r", "%s/%s", info->repodir, configfile))) {
		struct configitem keys[] = {
			{ "description", ConfigString, &info->description },
			{ "url", ConfigString, &info->cloneurl },
			{ "cloneurl", ConfigString, &info->cloneurl },
			{ "branch", ConfigString, &reqbranchname },
			/* { "revision", ConfigString, &info->revision }, */
			{ 0 },
		};

		info->confbuffer = parseconfig(fp, keys);
		fclose(fp);
	}

	/* check pinfiles */
	for (int i = 0; pinfiles[i] && info->pinfileslen < (int) LEN(info->pinfiles); i++) {
		addpinfile(info, pinfiles[i]);
	}

	if (extrapinfiles) {
		start = extrapinfiles;

		/* Loop through the string, finding each space and treating it as a delimiter */
		while (info->pinfileslen < (int) LEN(info->pinfiles) &&
		       (end = strchr(start, ' ')) != NULL) {
			*end = '\0'; /* Replace the space with a null terminator to isolate the token */
			addpinfile(info, start);
			start = end + 1;
		}

		addpinfile(info, start);
	}

	if (!git_revparse_single(&obj, info->repo, "HEAD:.gitmodules")) {
		if (git_object_type(obj) == GIT_OBJECT_BLOB)
			info->submodules = ".gitmodules";
		git_object_free(obj);
	}

	if (reqbranchname) {
		if (git_reference_lookup(&branch, info->repo, reqbranchname)) {
			fprintf(stderr, "error: unable to fetch branch %s\n", reqbranchname);
			git_repository_head(&branch, info->repo);
		}
	} else {
		git_repository_head(&branch, info->repo);
	}
	if (!branch)
		fprintf(stderr, "error: unable to get HEAD\n");
	else
		getreference(&info->branch, branch);

	getrefs(info);
}

void freerepo(struct repoinfo* info) {
	/* cleanup */
	freerefs(info);
	freereference(&info->branch);
	git_repository_free(info->repo);
	free(info->confbuffer);
	freeheadfiles(info);
}
