#include "getinfo.h"
#include "hprintf.h"
#include "writer.h"

#include <git2/commit.h>

static void writesignature(FILE* fp, const git_signature* sig) {
	fprintf(fp, "{");
	hprintf(fp, "\"name\":\"%j\",", sig->name);
	hprintf(fp, "\"email\":\"%j\",", sig->email);
	hprintf(fp, "\"date\":\"%T\"", &sig->when);
	fprintf(fp, "}");
}

void writejsoncommit(FILE* fp, git_commit* commit, int first) {
	char                 oid[GIT_OID_SHA1_HEXSIZE + 1], parentoid[GIT_OID_SHA1_HEXSIZE + 1];
	const git_signature* author    = git_commit_author(commit);
	const git_signature* committer = git_commit_committer(commit);
	const char*          summary   = git_commit_summary(commit);
	const char*          message   = git_commit_message(commit);

	git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));
	git_oid_tostr(parentoid, sizeof(parentoid), git_commit_parent_id(commit, 0));

	if (!first)
		fprintf(fp, ",");
	fprintf(fp, "\"%s\":", oid);

	fprintf(fp, "{");
	fprintf(fp, "\"id\":\"%s\",", oid);
	if (*parentoid)
		fprintf(fp, "\"parent\":\"%s\",", parentoid);
	else
		fprintf(fp, "\"parent\":null,");
	if (author) {
		fprintf(fp, "\"author\":");
		writesignature(fp, author);
		fprintf(fp, ",");
	} else {
		fprintf(fp, "\"author\":null,");
	}
	if (committer) {
		fprintf(fp, "\"committer\":");
		writesignature(fp, committer);
		fprintf(fp, ",");
	} else {
		fprintf(fp, "\"committer\":null,");
	}
	if (summary)
		hprintf(fp, "\"summary\":\"%j\",", summary);
	else
		fprintf(fp, "\"summary\":null,");
	if (message)
		hprintf(fp, "\"message\":\"%j\"", message);
	else
		fprintf(fp, "\"message\":null");
	fprintf(fp, "}");
}

static void writejsonref(FILE* fp, struct referenceinfo* refinfo) {
	fprintf(fp, "{");
	hprintf(fp, "\"name\":\"%j\",", refinfo->refname);
	fprintf(fp, "\"commit\":");
	writejsoncommit(fp, refinfo->commit, 0);
	fprintf(fp, "}");
}

void writejsonrefs(FILE* json, const struct repoinfo* info) {
	int isbranch = 1, first = 1;
	fprintf(json, "\"branches\":[");

	for (int i = 0; i < info->nrefs; i++) {
		if (isbranch && info->refs[i].istag) {
			fprintf(json, "],\"tags\":[");
			isbranch = 0;
			first    = 1;
		}

		if (!first) {
			fprintf(json, ",\n");
			first = 0;
		}
		writejsonref(json, &info->refs[i]);
	}

	fprintf(json, "]");
}
