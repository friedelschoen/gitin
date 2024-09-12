#include "common.h"

#include "compat.h"

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>


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

void printtime(FILE* fp, const git_time* intime) {
	struct tm* intm;
	time_t     t;
	char       out[32];

	t = (time_t) intime->time + (intime->offset * 60);
	if (!(intm = gmtime(&t)))
		return;
	strftime(out, sizeof(out), "%a, %e %b %Y %H:%M:%S", intm);
	if (intime->offset < 0)
		fprintf(fp, "%s -%02d%02d", out, -(intime->offset) / 60, -(intime->offset) % 60);
	else
		fprintf(fp, "%s +%02d%02d", out, intime->offset / 60, intime->offset % 60);
}

void printtimeshort(FILE* fp, const git_time* intime) {
	struct tm* intm;
	time_t     t;
	char       out[32];

	t = (time_t) intime->time;
	if (!(intm = gmtime(&t)))
		return;
	strftime(out, sizeof(out), "%Y-%m-%d %H:%M", intm);
	fputs(out, fp);
}

/* Percent-encode, see RFC3986 section 2.1. */
void percentencode(FILE* fp, const char* s) {
	static char   tab[] = "0123456789ABCDEF";
	unsigned char uc;

	while (*s) {
		uc = *s;
		/* NOTE: do not encode '/' for paths or ",-." */
		if (uc < ',' || uc >= 127 || (uc >= ':' && uc <= '@') || uc == '[' || uc == ']') {
			putc('%', fp);
			putc(tab[(uc >> 4) & 0x0f], fp);
			putc(tab[uc & 0x0f], fp);
		} else {
			putc(uc, fp);
		}
		s++;
	}
}

/* Escape characters below as HTML 2.0 / XML 1.0. */
void xmlencode(FILE* fp, const char* s) {
	while (*s) {
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
			default:
				putc(*s, fp);
		}
		s++;
	}
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
