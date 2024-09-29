#include "arg.h"
#include "parseconfig.h"
#include "writer.h"

#include <signal.h>

int force = 0;

static void usage(const char* argv0) {
	fprintf(stderr,
	        "usage: %s [-d destdir] [-c cachefile | -l commits] [-u baseurl] "
	        "repodir\n",
	        argv0);
	exit(1);
}

int main(int argc, char** argv) {
	const char* self    = argv[0];
	const char* destdir = ".";
	int         update;

	ARGBEGIN
	switch (OPT) {
		case 'd':
			destdir = EARGF(usage(self));
			break;
		case 'f':
			force = 1;
			break;
		case 'u':
			update = 1;
			break;
		default:
			fprintf(stderr, "error: unknown option '-%c'\n", OPT);
			return 1;
	}
	ARGEND

	if (argc == 0)
		usage(self);

	signal(SIGPIPE, SIG_IGN);

	setconfig();

	/* do not search outside the git repository:
	   GIT_CONFIG_LEVEL_APP is the highest level currently */
	git_libgit2_init();
	for (int i = 1; i <= GIT_CONFIG_LEVEL_APP; i++)
		git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, i, "");
	/* do not require the git repository to be owned by the current user */
	git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);

	if (!update) {
		writeindex(destdir, argv, argc);
	} else {
		for (int i = 0; i < argc; i++)
			writerepo(NULL, argv[i], destdir);
	}

	git_libgit2_shutdown();

	return 0;
}
