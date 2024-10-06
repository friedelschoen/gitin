#include "arg.h"
#include "config.h"
#include "hprintf.h"
#include "parseconfig.h"
#include "writer.h"

#include <dirent.h>
#include <git2/config.h>
#include <git2/global.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


int force = 0, verbose = 0, columnate = 0;

static int checkrepo(const char* path) {
	char        git_path[PATH_MAX];
	struct stat statbuf;

	// Check for bare repository (HEAD file directly in the directory)
	snprintf(git_path, sizeof(git_path), "%s/HEAD", path);
	if (stat(git_path, &statbuf) != 0 || !S_ISREG(statbuf.st_mode))
		return 0;

	// Check for config file in the git directory (non-bare repo)
	snprintf(git_path, sizeof(git_path), "%s/%s", path, configfile);
	if (stat(git_path, &statbuf) != 0 || !S_ISREG(statbuf.st_mode))
		return 0;

	return 1;
}

static void findrepos(const char* base_path, char*** repos, int* size) {
	struct dirent* dp;
	DIR*           dir = opendir(base_path);

	// Unable to open directory
	if (!dir)
		return;

	while ((dp = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		if (base_path[0] == '.' && base_path[1] == '\0')
			strncpy(path, dp->d_name, sizeof(path));
		else
			snprintf(path, sizeof(path), "%s/%s", base_path, dp->d_name);

		// Skip "." and ".." directories
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		// Check if it's a directory
		struct stat statbuf;
		if (stat(path, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode))
			continue;

		// Check if it's a Git repository (bare or non-bare)
		if (checkrepo(path)) {
			*repos = realloc(*repos, (*size + 1) * sizeof(char*));
			if (!*repos) {
				hprintf(stderr, "error: memory allocation: %w");
				exit(EXIT_FAILURE);
			}

			(*repos)[(*size)++] = strdup(path);
		} else {
			findrepos(path, repos, size);
		}
	}

	closedir(dir);
}

static void usage(const char* argv0, int exitcode) {
	fprintf(stderr,
	        "usage: %s [-fhu] [-C workdir] [-d destdir] repos...\n"
	        "   or: %s [-fhu] [-C workdir] [-d destdir] -r startdir\n",
	        argv0, argv0);
	exit(exitcode);
}

int main(int argc, char** argv) {
	const char* self    = argv[0];
	const char *destdir = ".", *pwd = NULL;
	int         update = 0, recursive = 0;
	char**      repos        = NULL;
	int         nrepos       = 0;
	char*       configbuffer = NULL;
	FILE*       config;

	ARGBEGIN
	switch (OPT) {
		case 'C':
			pwd = EARGF(usage(self, 1));
			break;
		case 'd':
			destdir = EARGF(usage(self, 1));
			break;
		case 'f':
			force = 1;
			break;
		case 'h':
			usage(self, 0);
		case 'r':
			recursive = 1;
			break;
		case 's':
			columnate = 1;
			break;
		case 'u':
			update = 1;
			break;
		case 'V':
			printf("gitin v%s\n", VERSION);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "error: unknown option '-%c'\n", OPT);
			return 1;
	}
	ARGEND

	if (pwd) {
		if (chdir(pwd)) {
			hprintf(stderr, "error: unable to change directory: %w\n");
			return 1;
		}
	}

	signal(SIGPIPE, SIG_IGN);

	if ((config = fopen(configfile, "r"))) {
		configbuffer = parseconfig(config, config_keys);
		fclose(config);
	}

	if (recursive) {
		if (argc == 0) {
			findrepos(".", &repos, &nrepos);
		} else if (argc == 1) {
			findrepos(argv[0], &repos, &nrepos);
		} else {
			fprintf(stderr, "error: too many arguments\n");
			return 1;
		}
	} else {
		if (argc == 0)
			usage(self, 1);

		repos  = argv;
		nrepos = argc;
	}

	/* do not search outside the git repository:
	   GIT_CONFIG_LEVEL_APP is the highest level currently */
	git_libgit2_init();
	for (int i = 1; i <= GIT_CONFIG_LEVEL_APP; i++)
		git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, i, "");
	/* do not require the git repository to be owned by the current user */
	git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);

	if (!update) {
		writeindex(destdir, repos, nrepos);
	} else {
		for (int i = 0; i < nrepos; i++) {
			writerepo(NULL, repos[i], destdir);
		}
	}

	git_libgit2_shutdown();

	if (recursive) {
		for (int i = 0; i < nrepos; i++) {
			free(repos[i]);
		}
		free(repos);
	}

	if (configbuffer)
		free(configbuffer);

	return 0;
}
