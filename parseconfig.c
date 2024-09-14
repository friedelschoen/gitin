#include "parseconfig.h"

#include "compat.h"

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

int parseconfig(struct configstate* state, FILE* fp) {
	for (;;) {
		if (getline(&state->line, &state->len, fp) == -1) {
			parseconfig_free(state);
			return -1;
		}

		if ((state->value = strchr(state->line, '#')) || (state->value = strchr(state->line, ';')))
			*state->value = '\0';

		if ((state->value = strchr(state->line, '='))) {
			*state->value++ = '\0';
			strlcpy(state->key, strip(state->line), sizeof(state->key));
		} else
			state->value = state->line;

		state->value = strip(state->value);

		if (!*state->value)
			continue;

		return 0;
		//		return handler(state->key, state->value, userdata);
	}
}

void parseconfig_free(struct configstate* state) {
	if (state->line)
		free(state->line);
}
