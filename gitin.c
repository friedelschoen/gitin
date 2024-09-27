#include "arg.h"
#include "config.h"
#include "writer.h"

#include <signal.h>


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

	ARGBEGIN
	switch (OPT) {
		case 'd':
			destdir = EARGF(usage(self));
			break;
		case 'h':
			highlightcmd = EARGF(usage(self));
			break;
		case 'c':
			commitcache = EARGF(usage(self));
			break;
		case 'H':
			highlightcache = EARGF(usage(self));
			break;
		case 'l':
			nlogcommits = atoi(EARGF(usage(self)));
			break;
	}
	ARGEND

	if (argc == 0)
		usage(self);

	signal(SIGPIPE, SIG_IGN);

	/* do not search outside the git repository:
	   GIT_CONFIG_LEVEL_APP is the highest level currently */
	git_libgit2_init();
	for (int i = 1; i <= GIT_CONFIG_LEVEL_APP; i++)
		git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, i, "");
	/* do not require the git repository to be owned by the current user */
	git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);

	writeindex(destdir, argv, argc);

	git_libgit2_shutdown();

	return 0;
}
