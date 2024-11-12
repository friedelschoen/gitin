#include "matchcapture.h"

#include <stdio.h>
#include <stdlib.h>

#define MAXCAPTURES 16

int main(int argc, char** argv) {
	char* captures[MAXCAPTURES];
	int   ncaptures;

	if (argc != 3) {
		fprintf(stderr, "usage: gitin-matchcapture <capture> <test>\n");
		return 1;
	}

	ncaptures = matchcapture(argv[2], argv[1], captures, MAXCAPTURES);
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
