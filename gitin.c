
#include "arg.h"
#include "composer.h"
#include "config.h"
#include "getinfo.h"
#include "hprintf.h"

#include <dirent.h>
#include <git2/config.h>
#include <git2/global.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


int force = 0, verbose = 0, columnate = 0, quiet = 0;
int archivezip = 0, archivetargz = 0, archivetarxz = 0, archivetarbz2 = 0;

static __attribute__((noreturn)) void usage(const char* argv0, int exitcode) {
	fprintf(stderr, "usage: %s [-fhqsVv] [-C workdir] <repository>\n", argv0);
	exit(exitcode);
}

int main(int argc, char** argv) {
	const char*     self = argv[0];
	const char *    pwd = NULL, *repodir = NULL;
	char*           configbuffer = NULL;
	FILE*           config;
	struct repoinfo repoinfo;

	ARGBEGIN
	switch (OPT) {
		case 'C':
			pwd = EARGF(usage(self, 1));
			break;
		case 'f':
			force = 1;
			break;
		case 'h':
			usage(self, 0);
		case 'q':
			quiet = 1;
			break;
		case 's':
			columnate = 1;
			break;
		case 'V':
			printf("gitin v%s\n", VERSION);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf(stderr, "error: unknown option '-%c'\n", OPT);
			return 1;
	}
	ARGEND

	repodir = ".";
	if (argc > 0)
		repodir = argv[0];

	if (pwd) {
		if (chdir(pwd)) {
			hprintf(stderr, "error: unable to change directory: %w\n");
			return 1;
		}
	}

	signal(SIGPIPE, SIG_IGN);

	if ((config = fopen(configfile, "r"))) {
		configbuffer = parseconfig(config, configkeys);
		fclose(config);
	}

	archivetypes = 0;
	if (archivetargz)
		archivetypes |= ArchiveTarGz;
	if (archivetarxz)
		archivetypes |= ArchiveTarXz;
	if (archivetarbz2)
		archivetypes |= ArchiveTarBz2;
	if (archivezip)
		archivetypes |= ArchiveZip;

	/* do not search outside the git repository:
	   GIT_CONFIG_LEVEL_APP is the highest level currently */
	git_libgit2_init();
	for (int i = 1; i <= GIT_CONFIG_LEVEL_APP; i++)
		git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, i, "");
	/* do not require the git repository to be owned by the current user */
	git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);

	getrepo(&repoinfo, repodir);
	composerepo(&repoinfo);
	freerepo(&repoinfo);

	git_libgit2_shutdown();

	if (configbuffer)
		free(configbuffer);

	return 0;
}
