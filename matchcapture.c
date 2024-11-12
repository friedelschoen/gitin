#include "matchcapture.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int matchend(const char* input, const char* templat, char** captures, int maxcaptures,
                    matchcapture_test_t test, int testnum, const char** named, int var, int greedy);

static int matchone(const char* input, const char* templat, char** captures, int maxcaptures,
                    matchcapture_test_t test, const char** named);

static int matchend(const char* input, const char* templat, char** captures, int maxcaptures,
                    matchcapture_test_t test, int testnum, const char** named, int var,
                    int greedy) {

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
			if (testnum == -1 || !test || test(testnum, value)) {
				if (maxcaptures > 0)
					*captures = value;
				if (var != -1)
					named[var] = *captures;

				if ((ret = matchone(input, templat, captures + 1, maxcaptures - 1, test, named)) !=
				    -1)
					return ret + 1;

				if (var != -1)
					named[var] = NULL;
			}
			free(value);
		}
		if (!*input)
			break;
		input += greedy ? -1 : +1;
	}
	return -1;
}

static int matchone(const char* input, const char* templat, char** captures, int maxcaptures,
                    matchcapture_test_t test, const char** named) {
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

				if (testnum == -1 || !test || test(testnum, input)) {
					if (maxcaptures > 0)
						*captures = strdup(input);
					return 1;
				}
				return -1;
			} else if (templat[0] == '}' && templat[1] != '\0') {
				templat++;

				return matchend(input, templat, captures, maxcaptures, test, testnum, named, -1,
				                greedy);
			} else if ((testnum && templat[0] == '|' && isalpha(templat[1]) && templat[2] == '}') ||
			           (!testnum && isalpha(templat[0]) && templat[1] == '}')) {
				if (testnum)
					templat++; /* skip | */
				// Enforced placeholder with specific character (e.g., `{a}`)
				int var = tolower(templat[0]) - 'a';
				templat += 2;    // Skip over `a}`

				// Check if this placeholder has been encountered before
				if (named[var] == NULL) {
					return matchend(input, templat, captures, maxcaptures, test, testnum, named,
					                var, greedy);
				}

				if (strncmp(input, named[var], strlen(named[var])))
					return -1;

				if (maxcaptures > 0)
					*captures = strdup(named[var]);
				input += strlen(named[var]);

				if ((ret = matchone(input, templat, captures + 1, maxcaptures - 1, test, named)) !=
				    -1)
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
                 matchcapture_test_t test) {
	const char* named[26];    // For named captures (a-z)

	memset(named, 0, sizeof(named));    // Clear previous capturesputs

	return matchone(input, templat, captures, maxcaptures, test, named);
}

int matchcaptures(const char* input, const char** templats, char** captures, int maxcaptures,
                  int* pncaptures, matchcapture_test_t test) {
	const char* named[26];    // For named captures (a-z)
	int         ncaptures;

	for (int i = 0; templats[i] != NULL; i++) {
		memset(named, 0, sizeof(named));    // Clear previous capturesputs

		if ((ncaptures = matchone(input, templats[i], captures, maxcaptures, test, named)) == -1)
			continue;

		if (pncaptures)
			*pncaptures = ncaptures;

		return i;
	}
	return -1;
}
