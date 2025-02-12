#include "common.h"
#include "composer.h"
#include "config.h"
#include "hprintf.h"
#include "writer.h"

#include <limits.h>
#include <string.h>


static int writeindexline(FILE* fp, FILE* cachefp, const char* repodir, const char* name,
                          const char* description) {
	const git_signature* author = NULL;
	int                  ret    = 0;

	hprintf(fp, "<tr><td><a href=\"%s/\">%y</a></td><td>%y</td><td>", repodir, name, description);
	if (author)
		hprintf(fp, "%t", &author->when);
	fputs("</td></tr>", fp);

	fprintf(cachefp, "%s,%s,%s\n", repodir, name, description);

	return ret;
}

static void writecategory(FILE* index, const char* name, int len) {
	char  category[PATH_MAX], configpath[PATH_MAX];
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
			fprintf(stderr, "error: unable to parse config at %s\n", configpath);

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

void writeindex(FILE* fp, const struct gitininfo* info, int dorepo) {
	FILE*           cachefp;
	const char*     category    = NULL;
	int             categorylen = 0;
	struct repoinfo repoinfo;


	emkdirf("!%s/.cache", info->destdir);

	cachefp = efopen(".w", "%s/.cache/index", info->destdir);
	writeheader(fp, NULL, 0, 0, sitename, "%y", sitedescription);
	fputs("<table id=\"index\"><thead>\n"
	      "<tr><td>Name</td><td class=\"expand\">Description</td><td>Last changes</td></tr>"
	      "</thead><tbody>\n",
	      fp);

	for (int i = 0; i < info->nindexes; i++) {
		if (iscategory(info->indexes[i].repodir, &category, &categorylen))
			writecategory(fp, category, categorylen);

		if (!info->indexes[i].name) {
			getrepo(&repoinfo, info->destdir, info->indexes[i].repodir);

			writeindexline(fp, cachefp, info->indexes[i].repodir, repoinfo.name,
			               repoinfo.description);

			if (dorepo)
				composerepo(&repoinfo);

			freerefs(&repoinfo);
		} else {
			writeindexline(fp, cachefp, info->indexes[i].repodir, info->indexes[i].name,
			               info->indexes[i].description);
		}
	}
	fputs("</tbody>\n</table>", fp);
	writefooter(fp);
}
