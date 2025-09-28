#include "common.h"
#include "composer.h"
#include "config.h"
#include "writer.h"

void composerepo(const struct repoinfo* info) {
	FILE *fp, *json, *atom;

	if (!quiet) {
		if (columnate)
			printf("%s\t%s\n", info->name, info->repodir);
		else
			printf("updating '%s' -> %s\n", info->name, info->repodir);
	}

	for (int i = 0; i < info->nrefs; i++) {
		emkdirf("%s", info->refs[i].refname);

		fp = efopen("w", "%s/index.html", info->refs[i].refname);
		writesummary(fp, info, &info->refs[i]);
		fclose(fp);

		/* log for HEAD */
		fp   = efopen("w", "%s/log.html", info->refs[i].refname);
		json = efopen("w", "%s/log.json", info->refs[i].refname);
		atom = efopen("w", "%s/log.xml", info->refs[i].refname);
		writelog(fp, json, atom, info, &info->refs[i]);
		fclose(fp);
		fclose(json);
		fclose(atom);

		composefiletree(info, &info->refs[i]);
	}

	fp = efopen("w", "index.html");
	writeredirect(fp, "%s/", info->branch.refname);
	fclose(fp);

	fp = efopen("w", "atom.xml");
	writeatomrefs(fp, info);
	fclose(fp);

	fp = efopen("w", "index.json");
	writejsonrefs(fp, info);
	fclose(fp);
}
