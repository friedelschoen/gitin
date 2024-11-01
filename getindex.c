#include "buffer.h"
#include "common.h"
#include "getinfo.h"

#include <string.h>

static int sortindex(const void* leftp, const void* rightp) {
	const struct indexinfo* left  = (struct indexinfo*) leftp;
	const struct indexinfo* right = (struct indexinfo*) rightp;

	return strcmp(left->repodir, right->repodir);
}

static struct indexinfo* parsecache(char* buffer, int* count) {
	struct indexinfo* indexes = NULL;
	char*             start   = buffer;

	while (start) {
		/* Find the end of the line (newline or end of buffer) */
		char* end = strchr(start, '\n');
		if (end)
			*end = '\0'; /* Replace newline with null terminator */

		/* Parse the repodir */
		char* name = strchr(start, ',');
		if (!name)
			break;      /* Invalid line, no comma found */
		*name++ = '\0'; /* Null-terminate repodir field */

		/* Parse the name */
		char* description = strchr(name, ',');
		if (!description)
			break;             /* Invalid line, no second comma */
		*description++ = '\0'; /* Null-terminate name field */

		/* Allocate more space for the new index */
		indexes                     = realloc(indexes, (*count + 1) * sizeof(*indexes));
		indexes[*count].repodir     = start;
		indexes[*count].name        = name;
		indexes[*count].description = description;
		(*count)++;

		/* Move to the next line if there is one */
		if (!end)
			break;

		start = end + 1;
	}

	return indexes;
}

int getindex(struct gitininfo* info, const char* destdir, const char** repos, int nrepos) {
	FILE* cachefp;
	char* cache = NULL;

	memset(info, 0, sizeof(*info));
	info->destdir = destdir;

	/* parse cache */
	if ((cachefp = efopen("!.r", "%s/.cache/index", info->destdir)) &&
	    (cache = loadbuffermalloc(cachefp, NULL))) {

		info->indexes = parsecache(cache, &info->nindexes);

		fclose(cachefp);
	}

	/* fill cache with to update repos */
	for (int i = 0; i < nrepos; i++) {
		for (int j = 0; j < info->nindexes; j++) {
			if (!strcmp(repos[i], info->indexes[j].repodir)) {
				info->indexes[j].name        = NULL; /* to be filled */
				info->indexes[j].description = NULL; /* to be filled */
				goto nextrepo;
			}
		}
		/* not cached */
		info->indexes = realloc(info->indexes, (info->nindexes + 1) * sizeof(struct indexinfo));
		info->indexes[info->nindexes].repodir     = repos[i];
		info->indexes[info->nindexes].name        = NULL;
		info->indexes[info->nindexes].description = NULL;
		info->nindexes++;

	nextrepo:;
	}

	qsort(info->indexes, info->nindexes, sizeof(struct indexinfo), sortindex);

	return 1;
}

void freeindex(struct gitininfo* info) {
	free(info->cache);
	free(info->indexes);
}
