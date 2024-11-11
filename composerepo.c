#include "common.h"
#include "composer.h"
#include "config.h"
#include "writer.h"

void composerepo(const struct repoinfo* info) {
	FILE* fp;

	if (columnate)
		printf("%s\t%s\t%s\n", info->name, info->repodir, info->destdir);
	else
		printf("updating '%s' (at %s) -> %s\n", info->name, info->repodir, info->destdir);

	emkdirf("%s", info->destdir);

	for (int i = 0; i < info->nrefs; i++) {
		emkdirf("%s/%s", info->destdir, info->refs[i].refname);

		fp = efopen("w", "%s/%s/index.html", info->destdir, info->refs[i].refname);
		writesummary(fp, info, &info->refs[i]);
		fclose(fp);

		composelog(info, &info->refs[i]);
		composefiletree(info, &info->refs[i]);
	}

	fp = efopen("w", "%s/index.html", info->destdir);
	writeredirect(fp, "%s/", info->branchname);
	fclose(fp);

	fp = efopen("w", "%s/atom.xml", info->destdir);
	writeatomrefs(fp, info);
	fclose(fp);

	fp = efopen("w", "%s/index.json", info->destdir);
	writejsonrefs(fp, info);
	fclose(fp);
}
