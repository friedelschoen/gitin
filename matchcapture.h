#pragma once

/**
 * matchcapture_test_t - Test for a capture.
 *
 * This function pointer is used in `matchcapture` to validate specific segments of captured input.
 * It should return a non-null value if the test passes, and null if it fails.
 *
 * Parameters:
 *   @testnum    : The identifier of the specific test to perform.
 *   @input      : The input segment to test.
 *
 * Returns:
 *   - Non-null if the test passes, null if it fails.
 *
 * Example:
 * ```
 * int testmatch(int testnum, const char* input) {
 *   switch (testnum) {
 *     case 0: // must be alpha
 *       for (;*input;input++) {
 *         if (!isalpha(*input))
 *           return 0;
 *       }
 *       return 1;
 *     case 1: // choice of
 *       for (int i = 0; i < LEN(choices); i++) {
 *         if (!strcmp(choices[i], input))
 *           return 1;
 *       }
 *       return 0;
 *     case ...:
 *       // more tests
 *   }
 * }
 * ```
 */
typedef int (*matchcapture_test_t)(int testnum, const char* input);

/**
 * matchcapture - Match an input string against a single template pattern.
 *
 * This function matches an input string to a specified template pattern and extracts
 * placeholder values into a captures array. Templates can contain various types of
 * placeholders, including regular captures, named captures, and optional test-based
 * captures.
 *
 * Template Syntax:
 * - `{}`                : Regular capture. Captures any sequence of characters until the next
 *                         matching character in the template.
 * - `{a}`, `{b}`, etc.  : Named capture. Functions like `{}`, but content captured here must match
 *                         any other occurrences of the same name in the template (e.g., `{a}` must
 *                         match all other `{a}` placeholders).
 * - `{?}`, `{?a}`, etc. : Non-greedy capture. `{?}` matches as few characters as possible until the
 *                         next match point. Named non-greedy captures enforce similar behavior with
 *                         minimal character consumption.
 * - `{0}`, `{a|0}`, `{?0}`, `{?0|a}`, etc.
 *                       : Testing capture, which can be greedy or non-greedy. Calls `@test` on the
 *                         consumed text segment to validate it. If the test fails, the next capture
 *                         option is tested. If `@test` is NULL, the capture is automatically
 *                         accepted.
 *
 * Parameters:
 *   @input      : The input string to match.
 *   @templat    : The template pattern to match against the input.
 *   @captures   : An array to store captured values for each placeholder `{}`, `{a}`, etc.
 *                 Must have space for `maxcaptures` entries.
 *   @maxcaptures: The maximum number of capture slots available in `captures`.
 *   @test       : A function pointer to a test function, or NULL if no tests are needed.
 *
 * Returns:
 *   - The number of captures written, or -1 if the template does not match.
 *
 * Example:
 * ```
 * const char* input = "/path/to/resource";
 * const char* template = "/{}/{}/{}/";
 * char* captures[3];
 * int result = matchcapture(input, template, captures, 3, NULL);
 * if (result >= 0) {
 *     // Process captures
 *     for (int i = 0; i < result; i++) {
 *         printf("Capture %d: %s\n", i, captures[i]);
 *         free(captures[i]);
 *     }
 * }
 * ```
 */
int matchcapture(const char* input, const char* templat, char** captures, int maxcaptures,
                 matchcapture_test_t test);

/**
 * matchcaptures - Match an input string against multiple template patterns.
 *
 * This function iterates through a set of template patterns, attempting to match each one in
 * sequence against the input string. If a match is found, it extracts placeholder values into the
 * captures array and returns the index of the matched template in the provided list.
 *
 * Parameters:
 *   @input       : The input string to match.
 *   @templates   : An array of template patterns to match against the input. The array must end
 *                  with a NULL entry.
 *   @captures    : An array to store captured values for each `{}`, `{a}`, etc., placeholder.
 *                  Must have space for `maxcaptures` entries.
 *   @maxcaptures : The maximum number of capture slots available in `captures`.
 *   @ncaptures   : Pointer to an integer to store the number of captured values if a match is
 *                  found (optional, can be NULL).
 *   @test        : A function pointer to a test function to validate test-based captures, or NULL
 *                  if no testing is required.
 *
 * Returns:
 *   - The index of the matched template in the `templates` array, or -1 if no match is found.
 *
 * Example:
 * ```
 * const char* input = "/repo/commit/123.html";
 * const char* templates[] = { "/{}/", "/{}/commit/{}.html", NULL };
 * char* captures[2];
 * int ncaptures;
 * int result = matchcaptures(input, templates, captures, 2, &ncaptures, NULL);
 * if (result >= 0) {
 *     printf("Matched template index: %d\n", result);
 *     for (int i = 0; i < ncaptures; i++) {
 *         printf("Capture %d: %s\n", i, captures[i]);
 *         free(captures[i]);
 *     }
 * }
 * ```
 */
int matchcaptures(const char* input, const char** templates, char** captures, int maxcaptures,
                  int* ncaptures, matchcapture_test_t test);
