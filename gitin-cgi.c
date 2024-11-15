
#include "arg.h"
#include "common.h"
#include "composer.h"
#include "config.h"
#include "findrepo.h"
#include "getinfo.h"
#include "hprintf.h"
#include "matchcapture.h"
#include "path.h"
#include "writer.h"

#include <dirent.h>
#include <git2/commit.h>
#include <git2/config.h>
#include <git2/global.h>
#include <git2/object.h>
#include <git2/oid.h>
#include <git2/refs.h>
#include <git2/revparse.h>
#include <git2/types.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


int force = 0, verbose = 0, columnate = 0, quiet = 1;
int archivezip = 0, archivetargz = 0, archivetarxz = 0, archivetarbz2 = 0;

char** repos  = NULL;
int    nrepos = 0;

static __attribute__((noreturn)) void usage(int exitcode) {
	fprintf(stderr, "usage: gitin-cgi [-fhrsVv] [-C workdir] [-d destdir] <path>\n");
	exit(exitcode);
}

enum {
	PATH_ROOT,                /* / */
	PATH_STYLE,               /* /style.css */
	PATH_REPO,                /* /{0}/ */
	PATH_REPO_ATOM,           /* /{0}/atom.xml */
	PATH_REPO_COMMIT,         /* /{0}/commit/{}.html */
	PATH_REPO_JSON,           /* /{0}/index.json */
	PATH_REPO_BRANCH,         /* /{0}/{}/ */
	PATH_REPO_BRANCH_JSON,    /* /{0}/{}/branch.json */
	PATH_REPO_BRANCH_ATOM,    /* /{0}/{}/atom.xml */
	PATH_REPO_BRANCH_LOG,     /* /{0}/{}/log.html */
	PATH_REPO_BRANCH_ARCHIVE, /* /{0}/{a}/{a}.{} */
	PATH_REPO_BRANCH_BLOB,    /* /{0}/{}/blobs/{} */
	PATH_REPO_BRANCH_FILE,    /* /{0}/{}/files/{}.html */
};

static int capturetest(int testnr, const char* text, void* userdata) {
	(void) userdata;

	switch (testnr) {
		case 0: /* is repository */
			for (int i = 0; i < nrepos; i++)
				if (!strcmp(repos[i], text))
					return 1;
			return 0;
		default:
			return 0;
	}
}

static void write404(FILE* fp, int relpath) {
	writeheader(fp, NULL, relpath, 0, sitename, sitedescription);
	fprintf(
	    fp,
	    "<table>\n"
	    "    <tr>\n"
	    "        <td>\n"
	    "            <a href=\"/\"><img src=\"logo.svg\" alt=\"Logo\" width=\"50\" height=\"50\" /></a>\n"
	    "        </td>\n"
	    "        <td class=\"expand\">\n"
	    "            <h1>404 - Not Found</h1>\n"
	    "        </td>\n"
	    "    </tr>\n"
	    "</table>\n"
	    "<hr />\n"
	    "<div id=\"content\">\n"
	    "    <p>The page you requested could not be found. It might have been moved, deleted, or does not exist.</p>\n"
	    "    <p>Please check the URL for errors or go back to the <a href=\"/\">home page</a>.</p>\n"
	    "    <p>If you believe this is an error, please contact the site administrator.</p>\n"
	    "</div>\n");
	writefooter(fp);
}

int main(int argc, char** argv) {
	const char *     destdir = "cgi-repos", *pwd = NULL;
	char *           reqpath, *end;
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
	end = strchr(reqpath, '\0') - 1;
	while (*end == '/')
		*(end--) = '\0';

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
	/* do not require the git repository to be owned by the current user */
	git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);

	static const char* urls[] = {
		"",                        //
		"style.css",               //
		"{0}",                     // repo
		"{0}/atom.xml",            // repo
		"{0}/commit/{}.html",      // repo
		"{0}/index.json",          // repo
		"{0}/{}/",                 // repo, rev
		"{0}/{}/log.json",         // repo, rev
		"{0}/{}/log.xml",          // repo, rev
		"{0}/{}/log.html",         // repo, rev
		"{0}/{a}/{a}.{}",          // repo, rev, rev, archive
		"{0}/{}/blobs/{}",         // repo, rev, path
		"{0}/{}/files/{}.html",    // repo, rev, path
		NULL,
	};
	char*                captures[4];
	int                  ncaptures;
	struct repoinfo      repoinfo;
	struct commitinfo    ci;
	git_oid              id;
	git_commit*          commit;
	char                 path[PATH_MAX];
	int                  cachedcommit;
	FILE *               fp = NULL, *atom = NULL, *json = NULL;
	struct referenceinfo refinfo;
	git_reference*       ref;
	int                  relpath;

	int indx = matchcaptures(reqpath, urls, captures, 4, &ncaptures, capturetest, NULL);
	switch (indx) {
		case -1:
			relpath = 0;
			for (char* p = reqpath; *p; p++)
				relpath += *p == '/';

			printf("Status: 404 Not Found\n");
			printf("Content-Type: text/html\n\n");
			write404(stdout, relpath);
			break;
		case PATH_ROOT: /* / */
			printf("Content-Type: text/html\n\n");
			getindex(&info, destdir, (const char**) repos, nrepos);
			emkdirf("%s", destdir);
			writeindex(stdout, &info, 0);
			freeindex(&info);
			break;

		case PATH_STYLE: /* /style.css */
			printf("Content-Type: text/css\n\n");
			fp = efopen("r", "style.css");
			copyfile(stdout, fp);
			fclose(fp);
			break;

		case PATH_REPO: /* /{0}/ */
			printf("Content-Type: text/html\n\n");
			getrepo(&repoinfo, destdir, captures[0]);
			writeredirect(stdout, "%s/", repoinfo.branch.refname);
			freerepo(&repoinfo);
			break;
		case PATH_REPO_ATOM: /* /{0}/atom.xml */
			printf("Content-Type: application/atom+xml\n\n");
			getrepo(&repoinfo, destdir, captures[0]);
			writeatomrefs(stdout, &repoinfo);
			freerepo(&repoinfo);
			break;
		case PATH_REPO_COMMIT: /* /{0}/commit/{}.html */
			printf("Content-Type: text/html\n\n");
			getrepo(&repoinfo, destdir, captures[0]);
			git_oid_fromstr(&id, captures[1]);

			/* Lookup the current commit */
			if (git_commit_lookup(&commit, repoinfo.repo, &id)) {
				hprintf(stderr, "error: unable to lookup commit: %gw\n");

				freerepo(&repoinfo);
				break;
			}

			snprintf(path, sizeof(path), "%s/commit/%s.html", repoinfo.destdir, captures[1]);

			cachedcommit = !force && !access(path, F_OK);
			if (getdiff(&ci, &repoinfo, commit, cachedcommit) == -1) {
				hprintf(stderr, "error: unable to get diff: %gw\n");
				git_commit_free(commit);
				freerepo(&repoinfo);
				break;
			}

			if (cachedcommit) {
				fp = efopen("r", "%s", path);
				copyfile(stdout, fp);
				fclose(fp);
			} else {
				fp = efopen("w", "%s", path);
				writecommit(fp, &repoinfo, commit, &ci, 1);
				fclose(fp);
			}

			freediff(&ci);
			git_commit_free(commit);
			freerepo(&repoinfo);
			break;
		case PATH_REPO_JSON: /* /{0}/index.json */
			printf("Content-Type: text/json\n\n");
			getrepo(&repoinfo, destdir, captures[0]);
			writejsonrefs(stdout, &repoinfo);
			freerepo(&repoinfo);
			break;
		case PATH_REPO_BRANCH: /* /{0}/{}/ */
			printf("Content-Type: text/html\n\n");
			getrepo(&repoinfo, destdir, captures[0]);

			if (git_reference_lookup(&ref, repoinfo.repo, captures[1])) {
				hprintf(stderr, "stderr: unable to fetch ref: %gw");
				freerepo(&repoinfo);
				break;
			}

			if (getreference(&refinfo, ref)) {
				hprintf(stderr, "stderr: unable to get ref: %gw");
				git_reference_free(ref);
				freerepo(&repoinfo);
				break;
			}

			writesummary(stdout, &repoinfo, &refinfo);
			freereference(&refinfo);
			freerepo(&repoinfo);
			break;
		case PATH_REPO_BRANCH_LOG:  /* /{0}/{}/log.html */
		case PATH_REPO_BRANCH_ATOM: /* /{0}/{}/atom.xml */
		case PATH_REPO_BRANCH_JSON: /* /{0}/{}/branch.json */
			getrepo(&repoinfo, destdir, captures[0]);
			if (git_reference_lookup(&ref, repoinfo.repo, captures[1])) {
				hprintf(stderr, "stderr: unable to fetch ref: %gw");
				freerepo(&repoinfo);
				break;
			}

			if (getreference(&refinfo, ref)) {
				hprintf(stderr, "stderr: unable to get ref: %gw");
				git_reference_free(ref);
				freerepo(&repoinfo);
				break;
			}

			if (indx == PATH_REPO_BRANCH_LOG) {
				fp = stdout;
				printf("Content-Type: text/html\n\n");
			} else if (indx == PATH_REPO_BRANCH_JSON) {
				json = stdout;
				printf("Content-Type: text/json\n\n");
			} else if (indx == PATH_REPO_BRANCH_ATOM) {
				atom = stdout;
				printf("Content-Type: application/xml+atom\n\n");
			}
			writelog(fp, atom, json, &repoinfo, &refinfo);
			freereference(&refinfo);
			freerepo(&repoinfo);
			break;
		case PATH_REPO_BRANCH_ARCHIVE: /* /{0}/{a}/{a}.{} */

			// TODO
			break;
		case PATH_REPO_BRANCH_FILE: /* /{0}/{}/files/{} */
		case PATH_REPO_BRANCH_BLOB: /* /{0}/{}/blobs/{} */
			getrepo(&repoinfo, destdir, captures[0]);

			if (git_reference_lookup(&ref, repoinfo.repo, captures[1])) {
				hprintf(stderr, "stderr: unable to fetch ref: %gw");
				freerepo(&repoinfo);
				break;
			}

			if (getreference(&refinfo, ref)) {
				hprintf(stderr, "stderr: unable to get ref: %gw");
				git_reference_free(ref);
				freerepo(&repoinfo);
				break;
			}

			composefiletree(&repoinfo, &refinfo);

			printf("Content-Type: %s\n\n",
			       indx == PATH_REPO_BRANCH_FILE ? "text/html" : "text/plain");

			snprintf(path, sizeof(path),
			         indx == PATH_REPO_BRANCH_FILE ? "%s/%s/files/%s" : "%s/%s/blobs/%s",
			         repoinfo.destdir, refinfo.refname, captures[2]);

			fp = efopen("r", "%s", path);
			copyfile(stdout, fp);
			fclose(fp);

			freereference(&refinfo);
			freerepo(&repoinfo);
			break;
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
