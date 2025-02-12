#include "matchcapture.h"

#include <alloca.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct strslice {
	char* string;
	int   length;
};

#define SLICECLEAR(slc) ((slc).string = 0, (slc).length = 0)

#define SLICESET(slc, str, len) ((slc).string = (str), (slc).length = (len))

struct mc_context {
	matchcapture_test_t test;
	void*               userdata;
	struct strslice     named[26];
};

static int consume(struct mc_context* ctx, char* input, const char* templat, char** captures,
                   int maxcaptures, int testnum, int var, int greedy);

static int matchone(struct mc_context* ctx, char* input, const char* templat, char** captures,
                    int maxcaptures);

static int consume(struct mc_context* ctx, char* input, const char* templat, char** captures,
                   int maxcaptures, int testnum, int var, int greedy) {

	char* start = input;
	int   ret, buffer;

	if (*templat == '{') {
		fprintf(stderr, "error: unable to match two captures next to each other\n");
		return -1;
	}

	if (greedy)
		input = strchr(input, '\0'); /* end of string */

	while (input >= start) {
		if (*input != *templat)
			goto nomatch;

		if (testnum != -1 && ctx->test) {
			buffer = *input;
			*input = '\0';
			if (!ctx->test(testnum, start, ctx->userdata)) {
				*input = buffer;
				goto nomatch;
			}
			*input = buffer;
		}

		if (var != -1)
			SLICESET(ctx->named[var], start, input - start);

		if ((ret = matchone(ctx, input, templat, captures + 1, maxcaptures - 1)) != -1) {
			*input = '\0';
			if (maxcaptures > 0)
				*captures = start;
			return ret + 1;
		}

		if (var != -1)
			SLICECLEAR(ctx->named[var]);

	nomatch:
		if (!greedy) {
			if (!*input)
				break;

			input++;
		} else {
			input--;
		}
	}
	return -1;
}

static int matchone(struct mc_context* ctx, char* input, const char* templat, char** captures,
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

				/* just capture the remaining input */
				if (testnum == -1 || !ctx->test || ctx->test(testnum, input, ctx->userdata)) {
					if (maxcaptures > 0)
						*captures = input;
					return 1;
				}
				return -1;
			} else if (templat[0] == '}' && templat[1] != '\0') {
				templat++;

				return consume(ctx, input, templat, captures, maxcaptures, testnum, -1, greedy);
			} else if ((testnum != -1 && templat[0] == '|' && isalpha(templat[1]) &&
			            templat[2] == '}') ||
			           (testnum == -1 && isalpha(templat[0]) && templat[1] == '}')) {
				if (testnum != -1)
					templat++; /* skip | */

				int var = tolower(templat[0]) - 'a';
				templat += 2; /* skip over `a}` */

				/* check if this placeholder has been encountered before */
				if (!ctx->named[var].length) {
					return consume(ctx, input, templat, captures, maxcaptures, testnum, var,
					               greedy);
				}

				if (strncmp(input, ctx->named[var].string, ctx->named[var].length))
					return -1;

				if (maxcaptures > 0)
					*captures = ctx->named[var].string; /* expect it to be NUL-delimited when the
					                                       first capture succeed */

				input += ctx->named[var].length;

				if ((ret = matchone(ctx, input, templat, captures + 1, maxcaptures - 1)) != -1)
					return ret + 1;

				return -1;
			} else {
				fprintf(stderr, "error: invalid capture: %.*s\n", (int) (templat - start + 1),
				        start);
				return -1;
			}
		} else if (*templat != *input) {
			return -1;
		} else {
			templat++;
			input++;
		}
	}

	return (*input == '\0' && *templat == '\0') ? 0 : -1;
}

int matchcapture(char* input, const char* templat, char** captures, int maxcaptures,
                 matchcapture_test_t test, void* userdata) {
	struct mc_context ctx;

	ctx.test     = test;
	ctx.userdata = userdata;
	memset(ctx.named, 0, sizeof(ctx.named));

	return matchone(&ctx, input, templat, captures, maxcaptures);
}

int matchcaptures(char* input, const char** templats, char** captures, int maxcaptures,
                  int* pncaptures, matchcapture_test_t test, void* userdata) {
	struct mc_context ctx;
	int               ncaptures;

	memset(&ctx, 0, sizeof(ctx)); /* clear previous captures */
	ctx.test     = test;
	ctx.userdata = userdata;

	for (int i = 0; templats[i] != NULL; i++) {
		memset(ctx.named, 0, sizeof(ctx.named));
		printf("input: %s\n", input);

		if ((ncaptures = matchone(&ctx, input, templats[i], captures, maxcaptures)) == -1)
			continue;

		if (pncaptures)
			*pncaptures = ncaptures;

		return i;
	}
	return -1;
}
