#include "common.h"

#include "compat.h"

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>


/* Handle read or write errors for a FILE * stream */
void checkfileerror(FILE* fp, const char* name, int mode) {
	if (mode == 'r' && ferror(fp))
		errx(1, "read error: %s", name);
	else if (mode == 'w' && (fflush(fp) || ferror(fp)))
		errx(1, "write error: %s", name);
}


int mkdirp(const char* path) {
	char tmp[PATH_MAX], *p;

	if (strlcpy(tmp, path, sizeof(tmp)) >= sizeof(tmp))
		errx(1, "path truncated: '%s'", path);
	for (p = tmp + (tmp[0] == '/'); *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXO) < 0 && errno != EEXIST)
			return -1;
		*p = '/';
	}
	if (mkdir(tmp, S_IRWXU | S_IRWXG | S_IRWXO) < 0 && errno != EEXIST)
		return -1;
	return 0;
}

/* Escape characters below as HTML 2.0 / XML 1.0, ignore printing '\r', '\n' */
void xmlencodeline(FILE* fp, const char* s, size_t len) {
	size_t i;

	for (i = 0; *s && i < len; s++, i++) {
		switch (*s) {
			case '<':
				fputs("&lt;", fp);
				break;
			case '>':
				fputs("&gt;", fp);
				break;
			case '\'':
				fputs("&#39;", fp);
				break;
			case '&':
				fputs("&amp;", fp);
				break;
			case '"':
				fputs("&quot;", fp);
				break;
			case '\r':
				break; /* ignore CR */
			case '\n':
				break; /* ignore LF */
			default:
				putc(*s, fp);
		}
	}
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
