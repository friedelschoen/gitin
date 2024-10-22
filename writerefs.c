#include "gitin.h"

#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/refs.h>
#include <git2/types.h>
#include <limits.h>
#include <string.h>


struct referenceinfo {
	git_reference* ref;
	git_commit*    commit;
};

static int refs_cmp(const void* v1, const void* v2) {
	const struct referenceinfo *r1 = v1, *r2 = v2;
	time_t                      t1, t2;

	t1 = git_commit_time(r1->commit);
	t2 = git_commit_time(r2->commit);

	if (t1 != t2)
		return t2 - t1;

	return strcmp(git_reference_shorthand(r1->ref), git_reference_shorthand(r2->ref));
}

static int writeref(FILE* fp, FILE* atom, FILE* json, const struct repoinfo* info,
                    struct referenceinfo* refs, size_t nrefs, const char* title) {
	char                 escapename[NAME_MAX], oid[GIT_OID_SHA1_HEXSIZE + 1];
	const char *         name, *summary, *refname;
	const git_signature* author;
	git_reference*       ref;
	git_commit*          commit;

	fprintf(fp,
	        "<h2>%s</h2><table>"
	        "<thead>\n<tr><td class=\"expand\">Name</td>"
	        "<td>Last commit date</td>"
	        "<td>Author</td>\n</tr>\n"
	        "</thead><tbody>\n",
	        title);

	for (size_t i = 0; i < nrefs; i++) {
		ref     = refs[i].ref;
		commit  = refs[i].commit;
		refname = git_reference_shorthand(ref);

		writelog(info, refname, commit);
		writefiles(info, refname, commit);
		writecommitatom(atom, commit, refname);

		FORMASK(type, archivetypes) {
			writearchive(info, type, refname, commit);
		}

		if (i > 0)
			fprintf(json, ",\n");
		writejsonref(json, info, ref, commit);

		name    = refname;
		author  = git_commit_author(commit);
		summary = git_commit_summary(commit);
		git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));

		strlcpy(escapename, name, sizeof(escapename));
		for (char* p = escapename; *p; p++)
			if (*p == '/')
				*p = '-';

		hprintf(fp, "<tr><td>%y", name);
		fprintf(fp,
		        " <small>at \"<a href=\"commit/%s.html\">%s</a>\"</small>"
		        " <a href=\"%s.html\">log</a>"
		        " <a href=\"file/%s/index.html\">files</a>",
		        oid, summary, name, name);

		fputs("</td><td>", fp);
		if (author)
			hprintf(fp, "%t", &author->when);
		fputs("</td><td>", fp);
		if (author)
			hprintf(fp, "%y", author->name);
		fputs("</td></tr>\n", fp);
	}

	fputs("</tbody></table>\n", fp);

	return 0;
}

#define ADDREF(refs, nrefs)                                                     \
	{                                                                           \
		if (!(refs = realloc(refs, (nrefs + 1) * sizeof(*refs)))) {             \
			hprintf(stderr, "error: unable to alloc memory for \"" #refs "\""); \
			continue;                                                           \
		}                                                                       \
                                                                                \
		refs[nrefs].ref    = ref;                                               \
		refs[nrefs].commit = commit;                                            \
		nrefs++;                                                                \
	}

static void freeref(struct referenceinfo* refs, size_t nrefs) {
	for (size_t i = 0; i < nrefs; i++) {
		git_commit_free(refs[i].commit);
		git_reference_free(refs[i].ref);
	}
	free(refs);
}

int writerefs(FILE* fp, FILE* json, const struct repoinfo* info) {
	struct referenceinfo*   branches  = NULL;
	struct referenceinfo*   tags      = NULL;
	size_t                  nbranches = 0, ntags = 0;
	git_reference_iterator* iter   = NULL;
	git_commit*             commit = NULL;
	git_reference*          ref    = NULL;
	FILE*                   atom;

	if (git_reference_iterator_new(&iter, info->repo))
		return -1;

	while (!git_reference_next(&ref, iter)) {
		if (!git_reference_is_branch(ref) && !git_reference_is_tag(ref)) {
			git_reference_free(ref);
			ref = NULL;
			continue;
		}

		if (git_reference_resolve(&ref, ref))
			continue;

		if (git_reference_peel((git_object**) &commit, ref, GIT_OBJECT_COMMIT))
			continue;

		if (git_reference_is_branch(ref)) {
			ADDREF(branches, nbranches);
		} else {    // is tag
			ADDREF(tags, ntags);
		}
	}
	git_reference_iterator_free(iter);

	/* sort by type, date then shorthand name */
	qsort(branches, nbranches, sizeof(*branches), refs_cmp);
	qsort(tags, ntags, sizeof(*tags), refs_cmp);

	atom = xfopen("w", "%s/%s", info->destdir, tagatomfile);
	writeatomheader(atom, info);

	fprintf(json, "\"branches\":[");

	if (nbranches)
		writeref(fp, atom, json, info, branches, nbranches, "Branches");

	fprintf(json, "],\"tags\":[");

	if (ntags)
		writeref(fp, atom, json, info, tags, ntags, "Tags");

	fprintf(json, "]");

	writeatomfooter(atom);
	fclose(atom);

	freeref(branches, nbranches);
	freeref(tags, ntags);

	return 0;
}
