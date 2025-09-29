#include "findrepo.h"

#include "config.h"
#include "hprintf.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


static int checkrepo(const char* path) {
	char        git_path[PATH_MAX];
	struct stat statbuf;

	/* Check for bare repository (HEAD file directly in the directory) */
	snprintf(git_path, sizeof(git_path), "%s/HEAD", path);
	if (stat(git_path, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
		return 1;

	/* Check for bare repository (HEAD file directly in the directory) */
	snprintf(git_path, sizeof(git_path), "%s/.git/HEAD", path);
	if (stat(git_path, &statbuf) == 0 && S_ISREG(statbuf.st_mode))
		return 1;

	return 0;
}

void findrepos(const char* base_path, char*** repos, int* size) {
	struct dirent* dp;
	DIR*           dir = opendir(base_path);

	/* Unable to open directory */
	if (!dir)
		return;

	while ((dp = readdir(dir)) != NULL) {
		char path[PATH_MAX];
		if (base_path[0] == '.' && base_path[1] == '\0')
			strlcpy(path, dp->d_name, sizeof(path));
		else
			snprintf(path, sizeof(path), "%s/%s", base_path, dp->d_name);

		/* Skip "." and ".." directories */
		if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
			continue;

		/* Check if it's a directory */
		struct stat statbuf;
		if (stat(path, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode))
			continue;

		/* Check if it's a Git repository (bare or non-bare) */
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
