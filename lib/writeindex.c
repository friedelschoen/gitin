#include "common.h"
#include "config.h"
#include "getinfo.h"
#include "hprintf.h"
#include "writer.h"

#include <git2/commit.h>
#include <git2/refs.h>
#include <git2/repository.h>
#include <git2/types.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>


static int writeindexline(FILE* fp, FILE* cachefp, struct referenceinfo* ref, const char* repodir,
                          const char* name, const char* description) {
	int ret = 0;

	// TODO: get time of HEAD
	hprintf(fp, "<tr><td><a href=\"%s/\">%y</a></td><td>%y</td><td>", repodir, name, description);
	if (ref) {
		hprintf(fp, "%t", git_commit_time(ref->commit));
		const git_signature* sig = git_commit_author(ref->commit);
		if (sig && sig->name)
			fprintf(fp, " by %s", sig->name);
	}
	fputs("</td></tr>", fp);

	fprintf(cachefp, "%s,%s,%s\n", repodir, name, description);

	return ret;
}

static void writecategory(FILE* index, const char* name, int len) {
	char  category[PATH_MAX];
	char* description = "";
	char* confbuffer  = NULL;
	FILE* fp;

	memcpy(category, name, len);
	category[len] = '\0';

	if ((fp = efopen("!r", "%s/%s", category, configfile))) {
		struct configitem keys[] = {
			{ "description", ConfigString, &description },
			{ 0 },
		};

		if (!(confbuffer = parseconfig(fp, keys)))
			fprintf(stderr, "error: unable to parse config at %s\n", configfile);

		fclose(fp);
	}

	hprintf(index, "<tr class=\"category\"><td>%y</td><td>%y</td><td></td></tr>\n", category,
	        description);

	free(confbuffer);
}

static int iscategory(const char* repodir, const char** category, int* categorylen) {
	int curlen = 0;

	if (strchr(repodir, '/'))
		curlen = strrchr(repodir, '/') - repodir;

	if (!*category || curlen != *categorylen || strncmp(*category, repodir, *categorylen) != 0) {
		*category    = repodir;
		*categorylen = curlen;
		return 1;
	}
	return 0;
}

void writeindex(FILE* fp, const struct gitininfo* info) {
	FILE*           cachefp;
	const char*     category    = NULL;
	int             categorylen = 0;
	struct repoinfo repoinfo;


	emkdirf("!.cache");

	cachefp = efopen(".w", ".cache/index");
	writeheader(fp, NULL, 0, 0, sitename, "%y", sitedescription);
	fputs("<table id=\"index\"><thead>\n"
	      "<tr><td>Name</td><td class=\"expand\">Description</td><td>Last changes</td></tr>"
	      "</thead><tbody>\n",
	      fp);

	for (int i = 0; i < info->nindexes; i++) {
		if (iscategory(info->indexes[i].repodir, &category, &categorylen))
			writecategory(fp, category, categorylen);

		if (!info->indexes[i].name) {
			getrepo(&repoinfo, info->indexes[i].repodir, 0);

			writeindexline(fp, cachefp, &repoinfo.branch, info->indexes[i].repodir, repoinfo.name,
			               repoinfo.description);

			freerepo(&repoinfo);
		} else {
			writeindexline(fp, cachefp, NULL, info->indexes[i].repodir, info->indexes[i].name,
			               info->indexes[i].description);
		}
	}
	fputs("</tbody>\n</table>", fp);
	writefooter(fp);
}
