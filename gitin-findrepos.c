
#include "config.h"
#include "findrepo.h"


int archivezip, archivetargz, archivetarxz, archivetarbz2; /* otherwise we have linker-errors */

int main(int argc, char* argv[]) {
	char** repos  = NULL;
	int    nrepos = 0;

	if (argc == 1) {
		findrepos(".", &repos, &nrepos);
	} else if (argc == 2) {
		findrepos(argv[0], &repos, &nrepos);
	} else {
		fprintf(stderr, "usage: gitin-findrepos [start]\n");
		return 1;
	}

	if (nrepos) {
		fprintf(stderr, "no repository found.\n");
		return 1;
	}

	for (int i = 0; i < nrepos; i++)
		printf("%s\n", repos[i]);

	return 0;
}
