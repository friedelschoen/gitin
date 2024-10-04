#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "parseconfig.h"
#include "writer.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


int writeindexline(FILE* fp, const struct repoinfo* info) {
	git_commit*          commit = NULL;
	const git_signature* author;
	git_revwalk*         w = NULL;
	git_oid              id;
	int                  ret = 0;

	git_revwalk_new(&w, info->repo);
	if (git_revwalk_push_head(w)) {
		hprintf(stderr, "error: unable to push head: %gw\n");
		return -1;
	}
	if (git_revwalk_next(&id, w) || git_commit_lookup(&commit, info->repo, &id)) {
		hprintf(stderr, "error: unable to lookup commit: %gw\n");
		ret = -1;
		goto err;
	}

	author = git_commit_author(commit);

	hprintf(fp, "<tr><td><a href=\"%s/\">%y</a></td><td>%y</td><td>", info->repodir, info->name,
	        info->description);
	if (author)
		hprintf(fp, "%t", &author->when);
	fputs("</td></tr>", fp);

	git_commit_free(commit);
err:
	git_revwalk_free(w);

	return ret;
}

static void writecategory(FILE* index, const char* name, int len) {
	char  category[PATH_MAX], configpath[PATH_MAX];
	char* description = "";
	char* confbuffer  = NULL;
	FILE* fp;

	memcpy(category, name, len);
	category[len] = '\0';

	if ((fp = xfopen("!r", "%s/%s", category, configfile))) {
		struct config keys[] = {
			{ "description", ConfigString, &description },
			{ 0 },
		};

		if (!(confbuffer = parseconfig(fp, keys)))
			fprintf(stderr, "error: unable to parse config at %s\n", configpath);

		fclose(fp);
	}

	hprintf(index, "<tr class=\"category\"><td>%y</td><td>%y</td><td></td></tr>\n", category,
	        description);

	free(confbuffer);
}

static int sortpath(const void* leftp, const void* rightp) {
	const char* left  = *(const char**) leftp;
	const char* right = *(const char**) rightp;

	return strcmp(left, right);
}

static void copyfile(const char* destdir, const char* dest, const char* srcpath) {
	char   destpath[PATH_MAX];
	FILE * srcfile, *destfile;
	char   buffer[4096];    // Buffer size for copying file content
	size_t n;

	// Construct the source and destination file paths
	snprintf(destpath, sizeof(destpath), "%s/%s", destdir, dest);

	// Open the source file for reading
	srcfile = fopen(srcpath, "rb");
	if (!srcfile) {
		hprintf(stderr, "error: unable to open source file %s: %w\n", srcpath);
		exit(100);
	}

	// Open the destination file for writing
	destfile = fopen(destpath, "wb");
	if (!destfile) {
		hprintf(stderr, "error: unable to create destination file %s: %w\n", destpath);
		fclose(srcfile);
		exit(100);
	}

	// Copy the file contents
	while ((n = fread(buffer, 1, sizeof(buffer), srcfile)) > 0) {
		if (fwrite(buffer, 1, n, destfile) != n) {
			hprintf(stderr, "error: unable to write to destination file %s: %w\n", destpath);
			fclose(srcfile);
			fclose(destfile);
			exit(100);
		}
	}

	if (ferror(srcfile))
		hprintf(stderr, "error: unable to read from source file %s: %w\n", srcpath);

	fclose(srcfile);
	fclose(destfile);
}

void writeindex(const char* destdir, char** repos, int nrepos) {
	FILE* index;

	xmkdirf(0777, "%s", destdir);

	if (copyfavicon)
		copyfile(destdir, favicon, copyfavicon);

	if (copylogoicon)
		copyfile(destdir, logoicon, copylogoicon);

	if (copystylesheet)
		copyfile(destdir, stylesheet, copystylesheet);

	index = xfopen("w+", "%s/index.html", destdir);
	writeheader(index, NULL, 0, sitename, "%y", sitedescription);
	fputs(
	    "<table id=\"index\"><thead>\n"
	    "<tr><td><b>Name</b></td><td class=\"expand\"><b>Description</b></td><td><b>Last changes</b></td></tr>"
	    "</thead><tbody>\n",
	    index);

	qsort(repos, nrepos, sizeof(char*), sortpath);

	const char* category    = NULL;
	int         categorylen = 0, curlen;
	for (int i = 0; i < nrepos; i++) {
		curlen = strchr(repos[i], '/') ? strrchr(repos[i], '/') - repos[i] : 0;
		if (!category || curlen != categorylen || strncmp(category, repos[i], categorylen) != 0) {
			category    = repos[i];
			categorylen = curlen;
			writecategory(index, category, categorylen);
		}

		writerepo(index, repos[i], destdir);
	}
	fputs("</tbody>\n</table>", index);
	writefooter(index);
	fclose(index);
}
