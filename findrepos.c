#include "config.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>


int check_git_repo(const char* path) {
	char        git_path[PATH_MAX];
	struct stat statbuf;

	// Check for bare repository (HEAD file directly in the directory)
	snprintf(git_path, sizeof(git_path), "%s/HEAD", path);
	if (stat(git_path, &statbuf) != 0 || !S_ISREG(statbuf.st_mode))
		return 0;

	// Check for bare repository (HEAD file directly in the directory)
	snprintf(git_path, sizeof(git_path), "%s/%s", path, configfile);
	if (stat(git_path, &statbuf) != 0 || !S_ISREG(statbuf.st_mode))
		return 0;

	return 1;
}

void recurse_directories(const char* base_path) {
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
		if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
			// Check if it's a Git repository (bare or non-bare)
			if (check_git_repo(path))
				printf("%s\n", path);
			else
				recurse_directories(path);
		}
	}

	closedir(dir);
}

int main(int argc, char* argv[]) {
	const char* start_path;

	if (argc == 1) {
		start_path = ".";    // Default to current directory
	} else if (argc == 2) {
		start_path = argv[1];    // Use path from argument
	} else {
		fprintf(stderr, "usage: findrepos [start-point]\n");
		return 1;
	}

	recurse_directories(start_path);
	return 0;
}
