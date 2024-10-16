#include "gitin.h"

#include <git2/commit.h>
#include <git2/revwalk.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


int writeindexline(FILE* fp, const struct repoinfo* info) {
	const git_signature* author = NULL;
	int                  ret    = 0;

	//	author = git_commit_author(commit);

	hprintf(fp, "<tr><td><a href=\"%s/\">%y</a></td><td>%y</td><td>", info->repodir, info->name,
	        info->description);
	if (author)
		hprintf(fp, "%t", &author->when);
	fputs("</td></tr>", fp);

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

void writeindex(const char* destdir, char** repos, int nrepos) {
	FILE* index;

	xmkdirf(0777, "%s", destdir);

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
