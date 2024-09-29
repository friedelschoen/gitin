#include "common.h"

#include <errno.h>
#include <ftw.h>
#include <stdio.h>


int mkdirp(char* path, int mode) {
	char* p;

	for (p = path + (path[0] == '/'); *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(path, mode) < 0 && errno != EEXIST)
			return -1;
		*p = '/';
	}
	if (mkdir(path, mode) < 0 && errno != EEXIST)
		return -1;
	return 0;
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
