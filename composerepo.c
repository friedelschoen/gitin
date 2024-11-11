#include "common.h"
#include "composer.h"
#include "config.h"
#include "writer.h"

void composerepo(const struct repoinfo* info) {
	FILE*       fp;
	const char* refname;

	if (columnate)
		printf("%s\t%s\t%s\n", info->name, info->repodir, info->destdir);
	else
		printf("updating '%s' (at %s) -> %s\n", info->name, info->repodir, info->destdir);

	emkdirf("%s", info->destdir);

	for (int i = 0; i < info->nrefs; i++) {
		refname = git_reference_shorthand(info->refs[i].ref);
		emkdirf("%s/%s", info->destdir, refname);

		fp = efopen("w", "%s/%s/index.html", info->destdir, refname);
		writesummary(fp, info, info->refs[i].ref, info->refs[i].commit);
		fclose(fp);

		composelog(info, info->refs[i].ref, info->refs[i].commit);
		composefiletree(info, info->refs[i].ref, info->refs[i].commit);
	}

	fp = efopen("w", "%s/index.html", info->destdir);
	writeredirect(fp, "%s/", git_reference_shorthand(info->branch));
	fclose(fp);

	fp = efopen("w", "%s/atom.xml", info->destdir);
	writeatomrefs(fp, info);
	fclose(fp);

	fp = efopen("w", "%s/index.json", info->destdir);
	writejsonrefs(fp, info);
	fclose(fp);
}
