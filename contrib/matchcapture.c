#include "matchcapture.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXCAPTURES 16

struct testcmds {
	char** arr;
	int    size;
};

int tester(int testindx, const char* input, void* ptestcmds) {
	struct testcmds* testcmds = ptestcmds;

	if (testindx >= testcmds->size)
		return 0;

	pid_t pid;

	if ((pid = fork()) == -1) {
		fprintf(stderr, "error: unable to fork: %s\n", strerror(errno));
		return 0;
	}

	if (pid == 0) {
		setenv("input", input, 1);
		execlp("sh", "sh", "-c", testcmds->arr[testindx], NULL);
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
	struct testcmds testcmds;
	char*           captures[MAXCAPTURES];
	int             ncaptures;

	if (argc < 3) {
		fprintf(stderr, "usage: %s <capture> <input> [test-commands...]\n", argv[0]);
		return 1;
	}


	testcmds.arr  = argv + 3;
	testcmds.size = argc - 3;

	ncaptures = matchcapture(argv[2], argv[1], captures, MAXCAPTURES, tester, &testcmds);
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
