#include "buffer.h"
#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "writer.h"

#include <limits.h>
#include <string.h>


static int writeindexline(FILE* fp, FILE* cachefp, struct indexinfo* info) {
	const git_signature* author = NULL;
	int                  ret    = 0;

	hprintf(fp, "<tr><td><a href=\"%s/\">%y</a></td><td>%y</td><td>", info->repodir, info->name,
	        info->description);
	if (author)
		hprintf(fp, "%t", &author->when);
	fputs("</td></tr>", fp);

	fprintf(cachefp, "%s,%s,%s\n", info->repodir, info->name, info->description);

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

static int sortindex(const void* leftp, const void* rightp) {
	const struct indexinfo* left  = (struct indexinfo*) leftp;
	const struct indexinfo* right = (struct indexinfo*) rightp;

	return strcmp(left->repodir, right->repodir);
}

struct indexinfo* parsecache(char* buffer, int* count) {
	struct indexinfo* indexes = NULL;
	char*             start   = buffer;

	while (start) {
		// Find the end of the line (newline or end of buffer)
		char* end = strchr(start, '\n');
		if (end)
			*end = '\0';    // Replace newline with null terminator

		// Parse the repodir
		char* name = strchr(start, ',');
		if (!name)
			break;         // Invalid line, no comma found
		*name++ = '\0';    // Null-terminate repodir field

		// Parse the name
		char* description = strchr(name, ',');
		if (!description)
			break;                // Invalid line, no second comma
		*description++ = '\0';    // Null-terminate name field

		// Allocate more space for the new index
		indexes                     = realloc(indexes, (*count + 1) * sizeof(*indexes));
		indexes[*count].repodir     = start;
		indexes[*count].name        = name;
		indexes[*count].description = description;
		(*count)++;

		// Move to the next line if there is one
		if (!end)
			break;

		start = end + 1;
	}

	return indexes;
}

void writeindex(const char* destdir, char** repos, int nrepos) {
	FILE *            fp, *cachefp;
	char*             cache     = NULL;
	size_t            cachesize = 0;
	struct indexinfo* indexes   = NULL;
	int               nindexes  = 0;
	emkdirf(0777, "%s", destdir);
	emkdirf(0777, "!%s/.cache", destdir);

	// parse cache
	if ((cachefp = efopen("!.r", "%s/.cache/index", destdir)) &&
	    (cache = loadbuffermalloc(cachefp, &cachesize))) {

		indexes = parsecache(cache, &nindexes);

		fclose(cachefp);
	}

	// fill cache with to update repos
	for (int i = 0; i < nrepos; i++) {
		for (int j = 0; j < nindexes; j++) {
			if (!strcmp(repos[i], indexes[j].repodir)) {
				indexes[j].name        = NULL;    // to be filled
				indexes[j].description = NULL;    // to be filled
				goto nextrepo;
			}
		}
		// not cached
		indexes                       = realloc(indexes, (nindexes + 1) * sizeof(struct indexinfo));
		indexes[nindexes].repodir     = repos[i];
		indexes[nindexes].name        = NULL;
		indexes[nindexes].description = NULL;
		nindexes++;

	nextrepo:;
	}

	qsort(indexes, nindexes, sizeof(struct indexinfo), sortindex);

	cachefp = efopen(".w", "%s/.cache/index", destdir);
	fp      = efopen("w+", "%s/index.html", destdir);
	writeheader(fp, NULL, 0, sitename, "%y", sitedescription);
	fputs("<table id=\"index\"><thead>\n"
	      "<tr><td>Name</td><td class=\"expand\">Description</td><td>Last changes</td></tr>"
	      "</thead><tbody>\n",
	      fp);

	const char* category    = NULL;
	int         categorylen = 0, curlen;
	for (int i = 0; i < nindexes; i++) {
		curlen = strchr(indexes[i].repodir, '/')
		           ? strrchr(indexes[i].repodir, '/') - indexes[i].repodir
		           : 0;
		if (!category || curlen != categorylen ||
		    strncmp(category, indexes[i].repodir, categorylen) != 0) {
			category    = indexes[i].repodir;
			categorylen = curlen;
			writecategory(fp, category, categorylen);
		}

		if (!indexes[i].name) {
			writerepo(&indexes[i], destdir);
			writeindexline(fp, cachefp, &indexes[i]);
			free(indexes[i].name);
			free(indexes[i].description);
		} else {
			writeindexline(fp, cachefp, &indexes[i]);
		}
	}
	fputs("</tbody>\n</table>", fp);
	writefooter(fp);
	fclose(fp);
	free(cache);
	free(indexes);
}
