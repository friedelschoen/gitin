#include "common.h"
#include "hprintf.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define RELPATHMAX 50

// Custom function similar to fprintf with %r and %h specifiers, manually handling %s, %d, %zd, %u, %zu, %c
void hprintf(FILE* file, const char* format, ...) {
	va_list args;
	va_start(args, format);

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
			normalize_path(dest);
			fprintf(file, "%s", dest);
		} else if (*p == 'y') {
			// Handle %y for xmlencoding
			const char* path = va_arg(args, const char*);
			xmlencode(file, path);


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

	va_end(args);
}
