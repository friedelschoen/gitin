#include "gitin.h"

#include <assert.h>
#include <errno.h>
#include <git2/errors.h>
#include <git2/types.h>
#include <stdarg.h>
#include <string.h>

#define RELPATHMAX 50


static void printtime(FILE* fp, const git_time* intime) {
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

static void printtimeshort(FILE* fp, const git_time* intime) {
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
static void percentencode(FILE* fp, const char* s) {
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
static void xmlencode(FILE* fp, const char* s) {
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

/* Escape characters below as HTML 2.0 / XML 1.0. */
static void jsonencode(FILE* fp, const char* s) {
	while (*s) {
		// \"
		// \\
    // \/
		// \b
		// \f
		// \n
		// \r
		// \t
		// \u followed by four-hex-digits

		switch (*s) {
			case '"':
				fputs("\\\"", fp);
				break;
			case '\\':
				fputs("\\\\", fp);
				break;
			case '/':
				fputs("\\/", fp);
				break;
			case '\b':
				fputs("\\b", fp);
				break;
			case '\f':
				fputs("\\f", fp);
				break;
			case '\n':
				fputs("\\n", fp);
				break;
			case '\r':
				fputs("\\r", fp);
				break;
			case '\t':
				fputs("\\t", fp);
				break;
			default:
				putc(*s, fp);
		}
		s++;
	}
}

void hprintf(FILE* file, const char* format, ...) {
	va_list args;
	va_start(args, format);
	vhprintf(file, format, args);
	va_end(args);
}

// Custom function similar to fprintf with %r and %h specifiers
void vhprintf(FILE* file, const char* format, va_list args) {
	for (const char* p = format; *p; p++) {
		if (*p != '%') {    // For normal characters, just print them
			fputc(*p, file);
			continue;
		}
		p++;    // Move past '%'

		if (*p == 'r') {
			// Handle %r for relative paths
			int count = va_arg(args, int);
			assert(count < RELPATHMAX);
			for (int i = 0; i < count; i++)
				fprintf(file, "../");
		} else if (*p == 'h') {
			// Handle %h for normalized paths
			const char* path = va_arg(args, const char*);
			char        dest[strlen(path) + 1];
			strcpy(dest, path);
			unhide_path(dest);
			fprintf(file, "%s", dest);
		} else if (*p == 'y') {
			// Handle %y for xmlencoding
			const char* path = va_arg(args, const char*);
			xmlencode(file, path);
		} else if (*p == 'Y') {
			// Handle %Y for percentencode
			const char* path = va_arg(args, const char*);
			percentencode(file, path);
		} else if (*p == 'T') {
			const git_time* time = va_arg(args, const git_time*);
			printtime(file, time);
		} else if (*p == 't') {
			const git_time* time = va_arg(args, const git_time*);
			printtimeshort(file, time);
		} else if (*p == 'w') {
			fprintf(file, "%s", strerror(errno));
		} else if (*p == 'W') {
			int error = va_arg(args, int);
			fprintf(file, "%s", strerror(error));
		} else if (strncmp(p, "gw", 2) == 0) {
			const git_error* error = git_error_last();
			fprintf(file, "%s", error ? error->message : "no error");
			p++;
		} else if (strncmp(p, "gW", 2) == 0) {
			const git_error* error = va_arg(args, const git_error*);
			fprintf(file, "%s", error ? error->message : "no error");
			p++;
		} else if (*p == 'j') {
			const char* text = va_arg(args, const char*);
			jsonencode(file, text);

		} else if (strncmp(p, "zd", 2) == 0) {
			// Handle %zd for signed size_t (use PRIdPTR from inttypes.h)
			ssize_t zd = va_arg(args, ssize_t);
			fprintf(file, "%zd", zd);
			p++;    // Move past 'd'
		} else if (*p == 'u') {
			// Handle %u for unsigned integers
			unsigned int u = va_arg(args, unsigned int);
			fprintf(file, "%u", u);
		} else if (strncmp(p, "zu", 2) == 0) {
			// Handle %zu for size_t (use PRIuPTR from inttypes.h)
			size_t zu = va_arg(args, size_t);
			fprintf(file, "%zu", zu);
			p++;    // Move past 'u'
		} else if (*p == 's') {
			// Handle %y for xmlencoding
			const char* text = va_arg(args, const char*);
			fprintf(file, "%s", text);
		} else if (*p == 'c') {
			// Handle %c for characters
			int c = va_arg(args, int);    // 'char' is promoted to 'int' in va_arg
			fputc(c, file);
		} else {
			// Handle unknown specifiers
			fprintf(file, "%%%c", *p);
		}
	}
}
