#include "matchcapture.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct mc_context {
	matchcapture_test_t test;
	void*               userdata;
	const char*         named[26];
};

static int matchend(struct mc_context* ctx, const char* input, const char* templat, char** captures,
                    int maxcaptures, int testnum, int var, int greedy);

static int matchone(struct mc_context* ctx, const char* input, const char* templat, char** captures,
                    int maxcaptures);

static int matchend(struct mc_context* ctx, const char* input, const char* templat, char** captures,
                    int maxcaptures, int testnum, int var, int greedy) {

	const char* start = input;
	char*       value;
	int         ret;

	if (*templat == '{') {
		fprintf(stderr, "error: adjacent captures without separators are not allowed\n");
		return -1;
	}

	if (greedy)
		input = strchr(input, '\0') - 1; /* end of string */

	while (input >= start) {
		if (*input == *templat) {
			value = strndup(start, input - start);
			if (testnum == -1 || !ctx->test || ctx->test(testnum, value, ctx->userdata)) {
				if (maxcaptures > 0)
					*captures = value;
				if (var != -1)
					ctx->named[var] = *captures;

				if ((ret = matchone(ctx, input, templat, captures + 1, maxcaptures - 1)) != -1)
					return ret + 1;

				if (var != -1)
					ctx->named[var] = NULL;
			}
			free(value);
		}
		if (!*input)
			break;
		input += greedy ? -1 : +1;
	}
	return -1;
}

static int matchone(struct mc_context* ctx, const char* input, const char* templat, char** captures,
                    int maxcaptures) {
	int         ret, greedy, testnum;
	const char* start;

	while (*templat && *input) {
		if (*templat == '{') {
			start = templat++;

			greedy = 1;
			if (*templat == '?') {
				greedy = 0;
				templat++;
			}

			testnum = -1;
			while (isdigit(*templat)) {
				if (testnum == -1) {
					testnum = *templat - '0';
				} else {
					testnum *= 10;
					testnum += *templat - '0';
				}
				templat++;
			}

			if (templat[0] == '}' && templat[1] == '\0') {
				templat++;

				if (testnum == -1 || !ctx->test || ctx->test(testnum, input, ctx->userdata)) {
					if (maxcaptures > 0)
						*captures = strdup(input);
					return 1;
				}
				return -1;
			} else if (templat[0] == '}' && templat[1] != '\0') {
				templat++;

				return matchend(ctx, input, templat, captures, maxcaptures, testnum, -1, greedy);
			} else if ((testnum && templat[0] == '|' && isalpha(templat[1]) && templat[2] == '}') ||
			           (!testnum && isalpha(templat[0]) && templat[1] == '}')) {
				if (testnum)
					templat++; /* skip | */
				// Enforced placeholder with specific character (e.g., `{a}`)
				int var = tolower(templat[0]) - 'a';
				templat += 2;    // Skip over `a}`

				// Check if this placeholder has been encountered before
				if (ctx->named[var] == NULL) {
					return matchend(ctx, input, templat, captures, maxcaptures, testnum, var,
					                greedy);
				}

				if (strncmp(input, ctx->named[var], strlen(ctx->named[var])))
					return -1;

				if (maxcaptures > 0)
					*captures = strdup(ctx->named[var]);
				input += strlen(ctx->named[var]);

				if ((ret = matchone(ctx, input, templat, captures + 1, maxcaptures - 1)) != -1)
					return ret + 1;

				if (maxcaptures > 0)
					free(*captures);
				return -1;
			} else {
				fprintf(stderr, "error: invalid capture: %.*s\n", (int) (templat - start), start);
				return -1;
			}
		} else if (*templat != *input) {
			return -1;
		} else {
			templat++;
			input++;
		}
	}

	if (*input == '\0' && *templat == '\0')
		return 0;

	return -1;
}

int matchcapture(const char* input, const char* templat, char** captures, int maxcaptures,
                 matchcapture_test_t test, void* userdata) {
	struct mc_context ctx;

	ctx.test     = test;
	ctx.userdata = userdata;
	memset(ctx.named, 0, sizeof(ctx.named));

	return matchone(&ctx, input, templat, captures, maxcaptures);
}

int matchcaptures(const char* input, const char** templats, char** captures, int maxcaptures,
                  int* pncaptures, matchcapture_test_t test, void* userdata) {
	struct mc_context ctx;
	int               ncaptures;

	memset(&ctx, 0, sizeof(ctx));    // Clear previous capturesputs
	ctx.test     = test;
	ctx.userdata = userdata;

	for (int i = 0; templats[i] != NULL; i++) {
		memset(ctx.named, 0, sizeof(ctx.named));

		if ((ncaptures = matchone(&ctx, input, templats[i], captures, maxcaptures)) == -1)
			continue;

		if (pncaptures)
			*pncaptures = ncaptures;

		return i;
	}
	return -1;
}
