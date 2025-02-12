#include "config.h"
#include "hprintf.h"
#include "path.h"

#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define MURMUR_SEED 0xCAFE5EED /* cafeseed */
#define fallthrough __attribute__((fallthrough));


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

void emkdirf(const char* format, ...) {
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
		if (mkdir(path, 0777) < 0 && errno != EEXIST)
			goto err;
		*p = '/';
	}
	if (mkdir(path, 0777) < 0 && errno != EEXIST)
		goto err;

	return;
err:
	hprintf(stderr, "error: unable to create directory %s: %w\n", path);
	exit(100);
}

static int removecallback(const char* fpath, const struct stat* sb, int typeflag,
                          struct FTW* ftwbuf) {
	(void) sb;
	(void) typeflag;
	(void) ftwbuf;

	return remove(fpath);
}

int removedir(char* path) {
	return nftw(path, removecallback, 64, FTW_DEPTH | FTW_PHYS);
}

void printprogress(ssize_t indx, ssize_t ncommits, const char* what, ...) {
	va_list args;
	if (verbose || quiet)
		return;

	printf("\r");
	va_start(args, what);
	vprintf(what, args);
	va_end(args);
	printf(" [");

	/* Handle zero commits case */
	if (ncommits == 0) {
		printf("##################################################] 100.0%% (0 / 0)");
		fflush(stdout);
		return;
	}

	double progress  = (double) indx / ncommits;
	int    bar_width = 50; /* The width of the progress bar */

	/* Print the progress bar */
	int pos = (int) (bar_width * progress);
	for (int i = 0; i < bar_width; ++i) {
		if (i < pos)
			fputc('#', stdout); /* Filled part of the bar */
		else
			fputc('-', stdout); /* Unfilled part of the bar */
	}

	/* Print the progress percentage and counts */
	printf("] %5.1f%% (%zd / %zd)", progress * 100, indx, ncommits);

	/* Ensure the line is flushed and fully updated in the terminal */
	fflush(stdout);
}

int issuffix(const char* str, const char* suffix) {
	size_t nstr    = strlen(str);
	size_t nsuffix = strlen(suffix);
	if (nsuffix > nstr)
		return 0;
	return strncmp(str + nstr - nsuffix, suffix, nsuffix) == 0;
}

int isprefix(const char* str, const char* prefix) {
	size_t nprefix = strlen(prefix);
	return strncmp(str, prefix, nprefix) == 0;
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

/* hash file using murmur3 */
u_int32_t filehash(const void* key, int len) {
	const uint8_t* data    = (const uint8_t*) key;
	const int      nblocks = len / 4;
	uint32_t       h1      = MURMUR_SEED;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	/* Body */
	const uint32_t* blocks = (const uint32_t*) (data + nblocks * 4);
	for (int i = -nblocks; i; i++) {
		uint32_t k1 = blocks[i];

		k1 *= c1;
		k1 = (k1 << 15) | (k1 >> (32 - 15));
		k1 *= c2;

		h1 ^= k1;
		h1 = (h1 << 13) | (h1 >> (32 - 13));
		h1 = h1 * 5 + 0xe6546b64;
	}

	/* Tail */
	const uint8_t* tail = (const uint8_t*) (data + nblocks * 4);
	uint32_t       k1   = 0;

	switch (len & 3) {
		case 3:
			k1 ^= tail[2] << 16;
			fallthrough;
		case 2:
			k1 ^= tail[1] << 8;
			fallthrough;
		case 1:
			k1 ^= tail[0];
			k1 *= c1;
			k1 = (k1 << 15) | (k1 >> (32 - 15));
			k1 *= c2;
			h1 ^= k1;
	}

	/* Finalization */
	h1 ^= len;
	h1 ^= h1 >> 16;
	h1 *= 0x85ebca6b;
	h1 ^= h1 >> 13;
	h1 *= 0xc2b2ae35;
	h1 ^= h1 >> 16;

	return h1;
}

char* escaperefname(char* refname) {
	for (char* p = refname; *p; p++)
		if (*p == '/')
			*p = '-';

	return refname;
}

int copyfile(FILE* dest, FILE* src) {
	uint8_t buffer[1024];
	size_t  nread;

	// Ensure both file pointers are valid
	if (!dest || !src) {
		errno = EINVAL;    // Invalid argument
		return -1;
	}

	// Copy data from src to dest
	while ((nread = fread(buffer, 1, sizeof(buffer), src)) > 0) {
		if (fwrite(buffer, 1, nread, dest) != nread) {
			return -1;    // Write error
		}
	}

	// Check for read error
	return ferror(src) ? -1 : 0;
}
