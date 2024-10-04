#include "common.h"

#include "config.h"
#include "hprintf.h"

#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


FILE* xfopen(const char* mode, const char* format, ...) {
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
		unhide_path(path);

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

void xmkdirf(int mode, const char* format, ...) {
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
		unhide_path(path);

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

void normalize_path(char* path) {
	char* src = path;
	char* dst = path;

	while (*src) {
		// Handle "//" by skipping over it
		if (src[0] == '/' && src[1] == '/') {
			src++;
			continue;
		}
		// Handle "/./" by skipping over it
		if (src[0] == '/' && src[1] == '.' && src[2] == '/') {
			src += 2;
			continue;
		}
		// Handle "/../" by removing the last component
		if (src[0] == '/' && src[1] == '.' && src[2] == '.' && src[3] == '/') {
			// Backtrack to the previous directory in dst
			if (dst != path) {
				dst--;
				while (dst > path && *dst != '/') {
					dst--;
				}
			}
			src += 3;
			continue;
		}
		// Copy the current character to the destination
		*dst++ = *src++;
	}
	// Null-terminate the result
	*dst = '\0';
}

void unhide_path(char* path) {
	for (char* chr = path; *chr; chr++) {
		if (*chr == '.' && (chr == path || chr[-1] == '/') && chr[1] != '/')
			*chr = '-';
	}
}
