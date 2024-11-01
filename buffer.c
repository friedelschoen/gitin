#include "buffer.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>

int loadbuffer(char* buffer, size_t len, const char* format, ...) {
	char    path[PATH_MAX];
	FILE*   fp;
	va_list list;

	va_start(list, format);
	vsnprintf(path, sizeof(path), format, list);
	va_end(list);

	if (!(fp = fopen(path, "r")))
		return -1;

	fread(buffer, len, 1, fp);
	fclose(fp);

	return 0;
}

char* loadbuffermalloc(FILE* fp, size_t* pbuflen) {
	size_t buflen;
	char*  buffer;
	fseek(fp, 0, SEEK_END);
	buflen = ftell(fp);
	rewind(fp);

	if (!(buffer = malloc(buflen + 1))) /* +1 for null terminator */
		return NULL;

	if (fread(buffer, 1, buflen, fp) != buflen) {
		free(buffer);
		return NULL;
	}

	for (size_t i = 0; i < buflen; i++) {
		if (!buffer[i]) {
			errno = EINVAL;
			free(buffer);
			return NULL;
		}
	}

	buffer[buflen] = '\0'; /* Null-terminate the buffer */
	if (pbuflen)
		*pbuflen = buflen;
	return buffer;
}

int writebuffer(const char* buffer, size_t len, const char* format, ...) {
	char    path[PATH_MAX];
	FILE*   fp;
	va_list list;

	va_start(list, format);
	vsnprintf(path, sizeof(path), format, list);
	va_end(list);

	if (!(fp = fopen(path, "w+")))
		return -1;

	fwrite(buffer, len, 1, fp);
	fclose(fp);

	return 0;
}
