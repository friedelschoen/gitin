#include "arg.h"
#include "config.h"
#include "findrepo.h"

#include <stdlib.h>
#include <unistd.h>


int archivezip, archivetargz, archivetarxz, archivetarbz2; /* otherwise we have linker-errors */

static __attribute__((noreturn)) void usage(int exitcode) {
	fprintf(stderr, "usage: gitin-findrepos [-C chdir] [start]\n");
	exit(exitcode);
}

int main(int argc, char* argv[]) {
	char** repos  = NULL;
	int    nrepos = 0;
	char*  start;

	ARGBEGIN
	switch (OPT) {
		case 'C':
			chdir(EARGF(usage(1)));
			break;
		case 'h':
			usage(0);
		default:
			fprintf(stderr, "error: unknown option '%d'\n", OPT);
			exit(1);
	}
	ARGEND

	if (argc == 0) {
		start = ".";
	} else if (argc == 1) {
		start = argv[0];
	} else {
		fprintf(stderr, "error: too many arguments\n");
		usage(1);
	}

	findrepos(start, &repos, &nrepos);

	if (!nrepos) {
		fprintf(stderr, "no repository found.\n");
		return 1;
	}

	for (int i = 0; i < nrepos; i++)
		printf("%s\n", repos[i]);

	for (int i = 0; i < nrepos; i++)
		free(repos[i]);
	free(repos);

	return 0;
}
