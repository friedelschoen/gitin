#include "matchcapture.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Example test function for testing captures. */
int test_function(int testnum, const char* input, void* userdata) {
	(void) userdata;    // Not used in this test
	switch (testnum) {
		case 0:    // Check if the input contains only digits
			for (int i = 0; input[i]; i++) {
				if (input[i] < '0' || input[i] > '9')
					return 0;
			}
			return 1;
		default:
			return 1;    // Default test passes
	}
}

/** Struct for test case definitions. */
typedef struct {
	const char* input;
	const char* template;
	int         expected_captures;
	const char* expected_results[4];
} testcase_t;

testcase_t test_cases[] = {
	// Basic cases
	{ "/repo/commit/123", "/{}/commit/{}", 2, { "repo", "123" } },
	{ "/repo/branch/branch1/branch1", "/{}/{a}/{a}", 3, { "repo/branch", "branch1", "branch1" } },
	{ "/path/to/resource", "/{}/to/{}", 2, { "path", "resource" } },
	{ "/123abc/to/456def", "/{?}/to/{?}", 2, { "123abc", "456def" } },

	// Named placeholders
	{ "/repo/branch/feature/feature", "/{}/{a}/{a}", 3, { "repo/branch", "feature", "feature" } },
	{ "/repo/branch/dev/main", "/{}/{a}/{b}", 3, { "repo/branch", "dev", "main" } },

	// Non-greedy matching
	{ "/path/to/something", "/{?}/to/{?}", 2, { "path", "something" } },
	{ "/long/path/to/some/resource", "/{}/to/{?}", 2, { "long/path", "some/resource" } },

	// Testing edge cases
	{ "/just/a/single/path", "/{}/a/{}", 2, { "just", "single/path" } },
	{ "/ends/with/slash/", "/{}/{}", 2, { "ends/with", "slash/" } },

	// Mismatches
	{ "/repo/commit123", "/{}/commit/{}", -1, { NULL } },
	{ "/repo/branch/branch1/branch2", "/{}/{a}/{a}", -1, { NULL } },

	// Overlapping named placeholders
	{ "/repo/branch1/branch2/branch1",
	  "/{}/{a}/{b}/{a}",
	  4,
	  { "repo", "branch1", "branch2", "branch1" } },
	{ "/repo/x/x/y", "/{}/{a}/{a}/{b}", 4, { "repo", "x", "x", "y" } },

	// Mixed greedy and non-greedy
	{ "/a/b/c/d", "/{?}/b/{?}", 2, { "a", "c/d" } },
	{ "/some/very/long/path/to/file", "/{}/to/{?}", 2, { "some/very/long/path", "file" } },

	// Placeholder with tests
	{ "/repo/123/branch/456", "/{}/123/{}/{?0}", 3, { "repo", "branch", "456" } },
	{ "/match/test/123", "/{}/test/{0}", 2, { "match", "123" } },

	{ 0 }    // End marker
};

int main(void) {
	char buffer[100];
	for (int i = 0; test_cases[i].input; i++) {
		char*       captures[4];
		testcase_t* tc = &test_cases[i];

		printf("test %d: matching '%s' with input '%s'\n", i + 1, tc->input, tc->template);

		strcpy(buffer, tc->input);
		int ncaptures = matchcapture(buffer, tc->template, captures, 4, test_function, NULL);

		printf(" -> got %d, expected %d captures\n", ncaptures, tc->expected_captures);
		assert(ncaptures == tc->expected_captures);

		for (int j = 0; j < ncaptures; j++) {
			printf("  capture %d: '%s', expected '%s'\n", j, captures[j], tc->expected_results[j]);

			assert(strcmp(captures[j], tc->expected_results[j]) == 0);
		}
		printf("\n");
	}

	printf("All tests passed.\n");
	return 0;
}
