#include "common.h"
#include "composer.h"
#include "config.h"
#include "writer.h"

void composerepo(const struct repoinfo* info) {
	FILE *fp, *json, *atom;

	if (!quiet) {
		if (columnate)
			printf("%s\t%s\t%s\n", info->name, info->repodir, info->destdir);
		else
			printf("updating '%s' (at %s) -> %s\n", info->name, info->repodir, info->destdir);
	}

	emkdirf("%s", info->destdir);

	for (int i = 0; i < info->nrefs; i++) {
		emkdirf("%s/%s", info->destdir, info->refs[i].refname);

		fp = efopen("w", "%s/%s/index.html", info->destdir, info->refs[i].refname);
		writesummary(fp, info, &info->refs[i]);
		fclose(fp);

		/* log for HEAD */
		fp   = efopen("w", "%s/%s/log.html", info->destdir, info->refs[i].refname);
		json = efopen("w", "%s/%s/log.json", info->destdir, info->refs[i].refname);
		atom = efopen("w", "%s/%s/log.xml", info->destdir, info->refs[i].refname);
		writelog(fp, json, atom, info, &info->refs[i]);
		fclose(fp);
		fclose(json);
		fclose(atom);

		composefiletree(info, &info->refs[i]);
	}

	fp = efopen("w", "%s/index.html", info->destdir);
	writeredirect(fp, "%s/", info->branch.refname);
	fclose(fp);

	fp = efopen("w", "%s/atom.xml", info->destdir);
	writeatomrefs(fp, info);
	fclose(fp);

	fp = efopen("w", "%s/index.json", info->destdir);
	writejsonrefs(fp, info);
	fclose(fp);
}
