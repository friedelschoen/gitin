#include "parseconfig.h"

#include "common.h"
#include "config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char* strip(char* str) {
	char* end;

	while (isspace(*str))
		str++;

	end = strchr(str, '\0') - 1;
	while (end > str && isspace(*end))
		*end-- = '\0';

	return str;
}

static int read_file_to_buffer(struct configstate* state, FILE* fp) {
	fseek(fp, 0, SEEK_END);
	state->buflen = ftell(fp);
	rewind(fp);

	state->buffer = malloc(state->buflen + 1);    // +1 for null terminator
	if (!state->buffer)
		return -1;

	if (fread(state->buffer, 1, state->buflen, fp) != state->buflen) {
		free(state->buffer);
		return -1;
	}

	state->buffer[state->buflen] = '\0';    // Null-terminate the buffer
	state->current               = state->buffer;
	return 0;
}

int parseconfig(struct configstate* state, FILE* file) {
	char *line, *comment;

	if (!state->buffer)
		read_file_to_buffer(state, file);

	for (;;) {
		if (!(line = strsep(&state->current, "\n")))
			return -1;    // End of buffer

		// Skip empty lines
		if (*line == '\0')
			continue;

		// Handle comments
		comment = strchr(line, '#');
		if (comment)
			*comment = '\0';

		comment = strchr(line, ';');
		if (comment)
			*comment = '\0';

		state->value = strchr(line, '=');
		if (state->value) {
			*state->value++ = '\0';    // Split key and value
			state->key      = strip(line);
			state->value    = strip(state->value);
		} else {
			// keep state->key
			state->value = strip(line);    // Current line is the value
		}

		if (*state->value && *state->key)
			return 0;    // Successfully parsed a key-value pair
	}
}

void parseconfig_free(struct configstate* state) {
	if (state->buffer)
		free(state->buffer);
}


static struct {
	const char*  name;
	const char** target;
} configs[] = {
	{ "name", &sitename },
	{ "description", &sitedescription },
	{ "footer", &footertext },
	{ "favicon", &favicon },
	{ "favicontype", &favicontype },
	{ "logoicon", &logoicon },
	{ "stylesheet", &stylesheet },
	{ "highlightcmd", &highlightcmd },
	{ "colorscheme", &colorscheme },
	{ "files/index", &indexfile },
	{ "files/log", &logfile },
	{ "files/files", &treefile },
	{ "files/json", &jsonfile },
	{ "files/commit-atom", &commitatomfile },
	{ "files/tag-atom", &tagatomfile },
};

static int setconfigarray(const char* key, const char* value) {
	for (int i = 0; i < (int) LEN(configs); i++) {
		if (!strcmp(key, configs[i].name)) {
			*configs[i].target = value;
			return 1;
		}
	}
	return 0;
}

void setconfig(void) {
	struct configstate state = { 0 };
	FILE*              fp;

	if ((fp = fopen(configfile, "r"))) {
		while (!parseconfig(&state, fp)) {
			if (!strcmp(state.key, "maxcommits"))
				maxcommits = atoi(state.value);
			else if (!strcmp(state.key, "maxfilesize"))
				maxfilesize = atoi(state.value);
			else if (!setconfigarray(state.key, state.value))
				fprintf(stderr, "warn: ignoring unknown config-key '%s'\n", state.key);
		}
		fclose(fp);
	}
}
