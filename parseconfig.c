#include "gitin.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef int bool;

static char* strip(char* str) {
	char* end;

	while (isspace((unsigned char) *str))    // Cast to unsigned char for isspace
		str++;

	end = strchr(str, '\0') - 1;
	while (end > str && isspace((unsigned char) *end))
		*end-- = '\0';

	return str;
}

static char* read_file_to_buffer(FILE* fp, size_t* pbuflen) {
	size_t buflen;
	char*  buffer;
	fseek(fp, 0, SEEK_END);
	buflen = ftell(fp);
	rewind(fp);

	if (!(buffer = malloc(buflen + 1)))    // +1 for null terminator
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

	buffer[buflen] = '\0';    // Null-terminate the buffer
	if (pbuflen)
		*pbuflen = buflen;
	return buffer;
}

static int boolstr(const char* str) {
	if (!strcmp(str, "true") || !strcmp(str, "yes"))
		return 1;
	else if (!strcmp(str, "false") || !strcmp(str, "no"))
		return 0;
	else
		return -1;
}

static int handle(struct config* keys, char* section, char* key, char* value) {
	char confkey[NAME_MAX];
	for (struct config* p = keys; p->name; p++) {
		if (section)
			snprintf(confkey, sizeof(confkey), "%s/%s", section, key);
		else
			strncpy(confkey, key, sizeof(confkey));

		if (!strcmp(p->name, confkey)) {
			switch (p->type) {
				case ConfigString:
					*(char**) p->target = value;
					break;
				case ConfigInteger:
					*(int*) p->target = atoi(value);
					break;
				case ConfigBoolean:
					if (boolstr(value) == -1) {
						fprintf(stderr,
						        "warn: '%s' is not a boolean value, leaving '%s' untouched.\n",
						        value, key);
					} else {
						*(bool*) p->target = boolstr(value);
					}
					break;
				case ConfigBooleanAuto:
					if (!strcmp(value, "auto")) {
						*(bool*) p->target = -1;
						break;
					}
					if (boolstr(value) == -1) {
						fprintf(stderr,
						        "warn: '%s' is not a boolean value, leaving '%s' untouched.\n",
						        value, key);
					} else {
						*(bool*) p->target = boolstr(value);
					}
					break;
			}
			return 1;
		}
	}
	return 0;
}

char* parseconfig(FILE* file, struct config* keys) {
	char *buffer, *line, *current, *value, *key, *section = NULL;

	if (!(buffer = read_file_to_buffer(file, NULL)))
		return NULL;

	current = buffer;
	while ((line = strsep(&current, "\n"))) {
		// Skip empty lines
		if (*line == '\0')
			continue;

		// Handle comments
		value = strchr(line, '#');
		if (value)
			*value = '\0';

		value = strchr(line, ';');
		if (value)
			*value = '\0';

		// Handle sections: [section]
		if (*line == '[' && line[strlen(line) - 1] == ']') {
			// Extract the section name
			line[strlen(line) - 1] = '\0';

			section = strip(line + 1);
			continue;
		}

		value = strchr(line, '=');
		if (!value)
			continue;

		*value++ = '\0';    // Split key and value
		key      = strip(line);
		value    = strip(value);

		// Skip lines without a valid key-value pair
		if (*key == '\0' || *value == '\0')
			continue;

		// Handle the key-value pair
		if (!handle(keys, section, key, value)) {
			fprintf(stderr, "warn: ignoring unknown key '%s'\n", key);
		}
	}

	return buffer;
}
