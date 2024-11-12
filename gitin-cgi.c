
#include "arg.h"
#include "common.h"
#include "config.h"
#include "findrepo.h"
#include "getinfo.h"
#include "hprintf.h"
#include "matchcapture.h"
#include "path.h"
#include "writer.h"

#include <dirent.h>
#include <git2/config.h>
#include <git2/global.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


int force = 0, verbose = 0, columnate = 0;
int archivezip = 0, archivetargz = 0, archivetarxz = 0, archivetarbz2 = 0;

static __attribute__((noreturn)) void usage(int exitcode) {
	fprintf(stderr, "usage: gitin-cgi [-fhrsVv] [-C workdir] [-d destdir] <path>\n");
	exit(exitcode);
}

int main(int argc, char** argv) {
	const char *     destdir = ".", *pwd = NULL;
	char*            reqpath;
	char**           repos        = NULL;
	int              nrepos       = 0;
	char*            configbuffer = NULL;
	int              stripleading = 0;
	FILE*            config;
	struct gitininfo info;

	ARGBEGIN
	switch (OPT) {
		case 'C':
			pwd = EARGF(usage(1));
			break;
		case 'd':
			destdir = EARGF(usage(1));
			break;
		case 'p':
			stripleading = atoi(EARGF(usage(1)));
			break;
		case 'f':
			force = 1;
			break;
		case 'h':
			usage(0);
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
			usage(1);
	}
	ARGEND

	if (argc != 1)
		usage(1);

	reqpath = argv[0];

	while (*reqpath == '/')
		reqpath++;

	if (!(reqpath = pathstrip(reqpath, stripleading))) {
		fprintf(stderr, "error: unable to strip %d components of path\n", stripleading);
		return 1;
	}

	pathnormalize(reqpath);

	if (pwd && chdir(pwd)) {
		hprintf(stderr, "error: unable to change directory: %w\n");
		return 1;
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

	findrepos(".", &repos, &nrepos);

	/* do not search outside the git repository:
	   GIT_CONFIG_LEVEL_APP is the highest level currently */
	git_libgit2_init();
	for (int i = 1; i <= GIT_CONFIG_LEVEL_APP; i++)
		git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, i, "");
	/* do not require the git repository to be owned by the current user */
	git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);

	static const char* urls[] = {
		"/",                       //
		"/style.css",              //
		"/{}/",                    // repo
		"/{}/atom.xml",            // repo
		"/{}/commit/{}.html",      // repo
		"/{}/index.json",          // repo
		"/{}/{}/",                 // repo, rev
		"/{}/{}/branch.json",      // repo, rev
		"/{}/{}/atom.xml",         // repo, rev
		"/{}/{}/log.html",         // repo, rev
		"/{}/{a}/{a}.{}",          // repo, rev, rev, archive
		"/{}/{}/blobs/{}",         // repo, rev, path
		"/{}/{}/files/{}.html",    // repo, rev, path
		NULL,
	};
	char* captures[4];
	int   ncaptures;

	struct repoinfo repoinfo;
	switch (matchcaptures(reqpath, urls, captures, 4, &ncaptures, NULL)) {
		case -1:
			fprintf(stderr, "404\n");
			break;
		case 0:
			getindex(&info, destdir, (const char**) repos, nrepos);
			emkdirf("%s", destdir);
			writeindex(stdout, &info, 0);
			freeindex(&info);
			break;
	}

	if (!*reqpath) {    // empty path
	} else {
		for (int i = 0; i < nrepos; i++) {
			if (!isprefix(repos[i], reqpath))
				continue;

			reqpath += strlen(repos[i]);
			while (*reqpath == '/')
				reqpath++;

			getrepo(&repoinfo, destdir, repos[i]);

			// todo
		}
	}

	for (int i = 0; i < ncaptures; i++)
		free(captures[i]);

	git_libgit2_shutdown();

	for (int i = 0; i < nrepos; i++)
		free(repos[i]);
	free(repos);

	if (configbuffer)
		free(configbuffer);

	return 0;
}
