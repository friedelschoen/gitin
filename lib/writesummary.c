#include "common.h"
#include "config.h"
#include "getinfo.h"
#include "writer.h"

#include <git2/blob.h>
#include <git2/commit.h>
#include <git2/tree.h>
#include <git2/types.h>
#include <stdio.h>
#include <string.h>

static git_blob* getcommitblob(const git_commit* commit, const char* path) {
	git_tree*       tree  = NULL;
	git_tree_entry* entry = NULL;
	git_blob*       blob  = NULL;

	if (git_commit_tree(&tree, commit)) {
		return NULL;
	}

	if (git_tree_entry_bypath(&entry, tree, path)) {
		git_tree_free(tree);
		return NULL;
	}

	if (git_tree_entry_type(entry) != GIT_OBJECT_BLOB) {
		git_tree_entry_free(entry);
		git_tree_free(tree);
		return NULL;
	}

	if (git_tree_entry_to_object((git_object**) &blob, git_commit_owner(commit), entry)) {
		git_tree_entry_free(entry);
		git_tree_free(tree);
		return NULL;
	}

	git_tree_entry_free(entry);
	git_tree_free(tree);

	return blob;
}

int writesummary(FILE* fp, const struct repoinfo* info, struct referenceinfo* refinfo) {
	const char*     readmename;
	git_blob*       readme = NULL;
	struct blobinfo blobinfo;

	/* log for HEAD */
	writeheader(fp, info, 1, 1, info->name, "%s", refinfo->refname);

	fprintf(fp, "<div id=\"refcontainer\">");
	writerefs(fp, info, 1, refinfo->ref);
	fprintf(fp, "</div>");

	fputs("<hr />", fp);

	if (clonepull) {
		fprintf(fp, "<h2>Clone</h2>");

		fprintf(fp, "<i>Pulling</i>");
		fprintf(fp, "<pre>git clone %s%s</pre>\n", clonepull, info->repodir);

		if (clonepush) {
			fprintf(fp, "<i>Pushing</i>");
			if (!strcmp(clonepull, clonepush)) {
				fprintf(fp, "<pre>git push origin %s</pre>\n", refinfo->refname);
			} else {
				fprintf(fp, "<pre>git remote add my-remote %s%s\ngit push my-remote %s</pre>\n",
				        clonepush, info->repodir, refinfo->refname);
			}
		}
	}

	for (int i = 0; aboutfiles[i]; i++) {
		readmename = aboutfiles[i];
		if ((readme = getcommitblob(refinfo->commit, aboutfiles[i])))
			break;
	}

	if (readme) {
		fprintf(fp, "<h2>About</h2>\n");

		blobinfo.name = readmename;
		blobinfo.path = readmename;
		blobinfo.blob = readme;
		blobinfo.hash = filehash(git_blob_rawcontent(readme), git_blob_rawsize(readme));

		writepreview(fp, 1, &blobinfo, 1);

		git_blob_free(readme);
	}

	writefooter(fp);

	return 0;
}
