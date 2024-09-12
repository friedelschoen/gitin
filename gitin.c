#include "arg.h"
#include "common.h"
#include "config.h"
#include "writer.h"

#include <err.h>
#include <signal.h>

static void usage(char* argv0) {
	fprintf(stderr,
	        "usage: %s [-d destdir] [-c cachefile | -l commits] [-u baseurl] "
	        "repodir\n",
	        argv0);
	exit(1);
}

int main(int argc, char* argv[]) {
	char* self = argv[0];
	FILE* index;

	ARGBEGIN
	switch (OPT) {
		case 'd':
			destination = EARGF(usage(self));
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

	mkdirp(highlightcache);

	/* do not search outside the git repository:
	   GIT_CONFIG_LEVEL_APP is the highest level currently */
	git_libgit2_init();
	for (int i = 1; i <= GIT_CONFIG_LEVEL_APP; i++)
		git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, i, "");
	/* do not require the git repository to be owned by the current user */
	git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);

	if (!(index = fopen(indexfile, "w+"))) {
		errx(1, "open index.html");
	}

	writeheader(index, NULL, "", "My Repositories", "Repositories!");
	fputs("<table id=\"index\"><thead>\n"
	      "<tr><td><b>Name</b></td><td><b>Description</b></td><td><b>Last changes</b></td></tr>"
	      "</thead><tbody>\n",
	      index);


	for (int i = 0; i < argc; i++)
		writerepo(index, argv[i]);

	fputs("</tbody>\n</table>", index);
	writefooter(index);
	checkfileerror(index, indexfile, 'w');
	fclose(index);

	git_libgit2_shutdown();

	return 0;
}
