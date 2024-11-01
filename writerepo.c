#include "common.h"
#include "config.h"
#include "writer.h"

void writerepo(const struct repoinfo* info) {
	FILE* fp;

	if (columnate)
		printf("%s\t%s\t%s\n", info->name, info->repodir, info->destdir);
	else
		printf("updating '%s' (at %s) -> %s\n", info->name, info->repodir, info->destdir);

	emkdirf(0777, "%s", info->destdir);
	emkdirf(0777, "!%s/.cache/archives", info->destdir);
	emkdirf(0777, "!%s/.cache/files", info->destdir);
	emkdirf(0777, "!%s/.cache/diffs", info->destdir);
	emkdirf(0777, "!%s/.cache/pandoc", info->destdir);
	emkdirf(0777, "!%s/.cache/blobs", info->destdir);
	emkdirf(0777, "%s/archive", info->destdir);
	emkdirf(0777, "%s/commit", info->destdir);

	for (int i = 0; i < info->nrefs; i++) {
		emkdirf(0777, "%s/%s", info->destdir, git_reference_shorthand(info->refs[i].ref));
		writelog(info, info->refs[i].ref, info->refs[i].commit);
		writefiletree(info, info->refs[i].ref, info->refs[i].commit);
	}

	fp = efopen("w", "%s/index.html", info->destdir);
	writeredirect(fp, git_reference_shorthand(info->branch));
	fclose(fp);

	fp = efopen("w", "%s/atom.html", info->destdir);
	writeatomrefs(fp, info);
	fclose(fp);

	fp = efopen("w", "%s/index.json", info->destdir);
	writejsonrefs(fp, info);
	fclose(fp);
}
