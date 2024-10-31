#include "config.h"
#include "hprintf.h"
#include "path.h"

#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


FILE* efopen(const char* mode, const char* format, ...) {
	char    path[PATH_MAX];
	FILE*   fp;
	va_list list;
	int     ignoreerr = 0, nounhide = 0;

	va_start(list, format);
	vsnprintf(path, sizeof(path), format, list);
	va_end(list);

	while (mode[0] == '!' || mode[0] == '.') {
		if (mode[0] == '!')
			ignoreerr = 1;
		if (mode[0] == '.')
			nounhide = 1;
		mode++;
	}

	if (!nounhide)
		pathunhide(path);

	if (!(fp = fopen(path, mode))) {
		if (ignoreerr)
			return NULL;

		hprintf(stderr, "error: unable to open file: %s: %w\n", path);
		exit(100);
	}
	if (verbose)
		fprintf(stderr, "%s\n", path);

	return fp;
}

void emkdirf(int mode, const char* format, ...) {
	char    path[PATH_MAX];
	char*   p;
	va_list args;
	int     nounhide = 0;

	if (format[0] == '!') {
		nounhide = 1;
		format++;
	}

	va_start(args, format);
	vsnprintf(path, sizeof(path), format, args);
	va_end(args);

	if (!nounhide)
		pathunhide(path);

	for (p = path + (path[0] == '/'); *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(path, mode) < 0 && errno != EEXIST)
			goto err;
		*p = '/';
	}
	if (mkdir(path, mode) < 0 && errno != EEXIST)
		goto err;

	return;
err:
	hprintf(stderr, "error: unable to create directory %s: %w\n", path);
	exit(100);
}

static int unlink_cb(const char* fpath, const struct stat* sb, int typeflag, struct FTW* ftwbuf) {
	(void) sb;
	(void) typeflag;
	(void) ftwbuf;

	return remove(fpath);
}

int removedir(char* path) {
	return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

void printprogress(ssize_t indx, ssize_t ncommits, const char* what, ...) {
	va_list args;
	if (verbose)
		return;

	printf("\r");
	va_start(args, what);
	vprintf(what, args);
	va_end(args);
	printf(" [");

	// Handle zero commits case
	if (ncommits == 0) {
		printf("##################################################] 100.0%% (0 / 0)");
		fflush(stdout);
		return;
	}

	double progress  = (double) indx / ncommits;
	int    bar_width = 50;    // The width of the progress bar

	// Print the progress bar
	int pos = (int) (bar_width * progress);
	for (int i = 0; i < bar_width; ++i) {
		if (i < pos)
			fputc('#', stdout);    // Filled part of the bar
		else
			fputc('-', stdout);    // Unfilled part of the bar
	}

	// Print the progress percentage and counts
	printf("] %5.1f%% (%zd / %zd)", progress * 100, indx, ncommits);

	// Ensure the line is flushed and fully updated in the terminal
	fflush(stdout);
}

int isprefix(const char* str, const char* suffix) {
	size_t lenstr    = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix > lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

const char* splitunit(ssize_t* size) {
	const char* unit = "B";
	if (*size > 1024) {
		unit = "KiB";
		*size /= 1024;
		if (*size > 1024) {
			unit = "MiB";
			*size /= 1024;
			if (*size > 1024) {
				unit = "GiB";
				*size /= 1024;
				if (*size > 1024) {
					unit = "TiB";
					*size /= 1024;
				}
			}
		}
	}
	return unit;
}
