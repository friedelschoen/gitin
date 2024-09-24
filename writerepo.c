#include "commit.h"
#include "common.h"
#include "compat.h"
#include "config.h"
#include "hprintf.h"
#include "makearchive.h"
#include "parseconfig.h"
#include "writer.h"

#include <err.h>
#include <git2/commit.h>
#include <git2/signature.h>
#include <git2/types.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>


static int writerefs(FILE* fp, const struct repoinfo* info, const git_oid* head) {
	struct referenceinfo* ris = NULL;
	const git_signature*  author;
	size_t                count, i, j, refcount;
	const char*           titles[] = { "Branches", "Tags" };
	const char*           ids[]    = { "branches", "tags" };
	const char*           s;
	int                   ishead;

	if (getrefs(&ris, &refcount, info->repo) == -1)
		return -1;

	fprintf(fp, "<div id=\"refcontainer\">\n");

	for (i = 0, j = 0, count = 0; i < refcount; i++) {
		if (j == 0 && git_reference_is_tag(ris[i].ref)) {
			if (count)
				fputs("</tbody></table></div>\n", fp);
			count = 0;
			j     = 1;
		}

		makearchive(info, ris[i].ref);
		ishead = head && !git_oid_cmp(git_reference_target(ris[i].ref), head);

		/* print header if it has an entry (first). */
		if (++count == 1) {
			fprintf(fp,
			        "<div class=\"ref\"><h2>%s</h2><table id=\"%s\">"
			        "<thead>\n<tr><td class=\"expand\"><b>Name</b></td>"
			        "<td><b>Last commit date</b></td>"
			        "<td><b>Author</b></td>\n</tr>\n"
			        "</thead><tbody>\n",
			        titles[j], ids[j]);
		}

		s      = git_reference_shorthand(ris[i].ref);
		author = git_commit_author(ris[i].commit);

		hprintf(fp, "<tr><td><a href=\"archive/%s.tar.gz\">", s);
		if (ishead)
			hprintf(fp, "<b>%y</b>", s);
		else
			hprintf(fp, "%y", s);
		hprintf(fp, "</a></td><td>", s);
		if (author)
			hprintf(fp, "%t", &author->when);
		fputs("</td><td>", fp);
		if (author)
			hprintf(fp, "%y", author->name);
		fputs("</td></tr>\n", fp);
	}
	/* table footer */
	if (count)
		fputs("</tbody></table></div>\n", fp);

	for (i = 0; i < refcount; i++) {
		git_commit_free(ris[i].commit);
		git_reference_free(ris[i].ref);
	}
	free(ris);

	fprintf(fp, "</div>\n");

	return 0;
}

void freeheadfiles(struct repoinfo* info) {
	if (info->headfiles != NULL) {
		// Free each string in the list
		for (int i = 0; i < info->headfileslen; i++) {
			free(info->headfiles[i]);
		}
		// Free the list itself
		free(info->headfiles);
		info->headfiles = NULL;
	}

	info->headfileslen   = 0;
	info->headfilesalloc = 0;
}

void writerepo(FILE* index, const char* repodir) {
	struct repoinfo info;

	git_object*    obj  = NULL;
	const git_oid* head = NULL;
	FILE*          fp;
	char           path[PATH_MAX], description[256];
	int            i;
	const char*    start;

	memset(&info, 0, sizeof(info));
	info.repodir = repodir;

	info.relpath = 1;
	for (const char* p = repodir + 1; p[1]; p++)
		if (*p == '/')
			info.relpath++;

	snprintf(info.destdir, sizeof(info.destdir), "%s%s", destination, info.repodir);
	normalize_path(info.destdir);

	if (mkdirp(info.destdir) == -1) {
		err(1, "cannot create destdir: %s", info.destdir);
		return;
	}

	if (git_repository_open_ext(&info.repo, info.repodir, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) < 0) {
		err(1, "cannot open repository");
		return;
	}

	/* find HEAD */
	if (!git_revparse_single(&obj, info.repo, "HEAD"))
		head = git_object_id(obj);
	git_object_free(obj);

	start = repodir;
	if (start[0] == '/')
		start++;
	/* use directory name as name */
	strlcpy(info.name, start, sizeof(info.name));
	if (info.name[strlen(info.name) - 1] == '/')
		info.name[strlen(info.name) - 1] = '\0';

	struct configstate state;
	memset(&state, 0, sizeof(state));
	snprintf(path, sizeof(path), "%s/%s", repodir, configfile);
	normalize_path(path);

	if ((fp = fopen(path, "r"))) {
		while (!parseconfig(&state, fp)) {
			if (!strcmp(state.key, "description"))
				strlcpy(info.description, state.value, sizeof(info.description));
			else if (!strcmp(state.key, "url") || !strcmp(state.key, "cloneurl"))
				strlcpy(info.cloneurl, state.value, sizeof(info.cloneurl));
			else
				fprintf(stderr, "warn: ignoring unknown config-key '%s'\n", state.key);
		}
	}

	/* check pinfiles */
	for (i = 0; i < npinfiles && info.pinfileslen < MAXPINS; i++) {
		snprintf(path, sizeof(path), "HEAD:%s", pinfiles[i]);
		if (!git_revparse_single(&obj, info.repo, path) && git_object_type(obj) == GIT_OBJ_BLOB)
			info.pinfiles[info.pinfileslen++] = pinfiles[i];
		git_object_free(obj);
	}

	if (!git_revparse_single(&obj, info.repo, "HEAD:.gitmodules") && git_object_type(obj) == GIT_OBJ_BLOB)
		info.submodules = ".gitmodules";
	git_object_free(obj);

	/* files for HEAD, it must be before writelog, as it also populates headfiles! */
	snprintf(path, sizeof(path), "%s/files.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	fprintf(stderr, "%s\n", path);
	snprintf(description, sizeof(description), "Files of %s", info.name);
	writeheader(fp, &info, 0, "Files", description);
	if (head)
		writefiles(fp, &info, head);
	writefooter(fp);
	checkfileerror(fp, path, 'w');
	fclose(fp);

	/* log for HEAD */
	snprintf(path, sizeof(path), "%s/index.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	fprintf(stderr, "%s\n", path);
	snprintf(path, sizeof(path), "%s/commit", info.destdir);
	mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
	snprintf(description, sizeof(description), "Commits of %s", info.name);
	writeheader(fp, &info, 0, info.name, description);
	writerefs(fp, &info, head);
	fputs("<h2>Commits</h2>\n<table id=\"log\"><thead>\n<tr><td><b>Date</b></td>"
	      "<td class=\"expand\"><b>Commit message</b></td>"
	      "<td><b>Author</b></td><td class=\"num\" align=\"right\"><b>Files</b></td>"
	      "<td class=\"num\" align=\"right\"><b>+</b></td>"
	      "<td class=\"num\" align=\"right\"><b>-</b></td></tr>\n</thead><tbody>\n",
	      fp);

	if (head)
		writelog(fp, &info);

	fputs("</tbody></table>", fp);
	writefooter(fp);
	snprintf(path, sizeof(path), "%s/index.html", info.destdir);
	checkfileerror(fp, path, 'w');
	fclose(fp);

	writeindex(index, &info);
	checkfileerror(index, "index.html", 'w');

	/* cleanup */
	git_repository_free(info.repo);
	freeheadfiles(&info);
}
