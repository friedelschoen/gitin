#include "matchcapture.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXCAPTURES 16

char** testcmds;
int    ntestcmds;

int tester(int testindx, const char* input) {
	if (testindx >= ntestcmds)
		return 0;

	pid_t pid;

	if ((pid = fork()) == -1) {
		fprintf(stderr, "error: unable to fork: %s\n", strerror(errno));
		return 0;
	}

	if (pid == 0) {
		setenv("input", input, 1);
		execlp("sh", "sh", "-c", testcmds[testindx], NULL);
		fprintf(stderr, "error: unable to execute test: %s\n", strerror(errno));
		_exit(EXIT_FAILURE);
	}

	int status;
	if (waitpid(pid, &status, 0) == -1) {
		fprintf(stderr, "error: unable to wait for process: %s\n", strerror(errno));
		return 0;
	}

	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(int argc, char** argv) {
	char* captures[MAXCAPTURES];
	int   ncaptures;

	if (argc < 3) {
		fprintf(stderr, "usage: gitin-matchcapture <capture> <input> [test-commands...]\n");
		return 1;
	}

	testcmds  = argv + 3;
	ntestcmds = argc - 3;

	ncaptures = matchcapture(argv[2], argv[1], captures, MAXCAPTURES, tester);
	if (ncaptures == -1) {
		printf("not a match\n");
		return 1;
	}

	for (int i = 0; i < ncaptures; i++) {
		printf("%d: %s\n", i, captures[i]);
		free(captures[i]);
	}

	return 0;
}
