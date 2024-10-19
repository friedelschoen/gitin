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
	int                         r;
	const git_signature *       author1, *author2;

	if ((r = git_reference_is_tag(r1->ref) - git_reference_is_tag(r2->ref)))
		return r;

	author1 = git_commit_author(r1->commit);
	author2 = git_commit_author(r2->commit);

	t1 = author1 ? author1->when.time : 0;
	t2 = author2 ? author2->when.time : 0;
	if ((r = t1 > t2 ? -1 : (t1 == t2 ? 0 : 1)))
		return r;

	return strcmp(git_reference_shorthand(r1->ref), git_reference_shorthand(r2->ref));
}

static int writeref(FILE* fp, FILE* atom, FILE* json, const struct repoinfo* info,
                    struct referenceinfo* refs, size_t nrefs, const char* title) {
	char                 escapename[NAME_MAX], oid[GIT_OID_SHA1_HEXSIZE + 1];
	const char *         name, *summary;
	const git_signature* author;

	fprintf(fp,
	        "<h2>%s</h2><table>"
	        "<thead>\n<tr><td class=\"expand\"><b>Name</b></td>"
	        "<td><b>Last commit date</b></td>"
	        "<td><b>Author</b></td>\n</tr>\n"
	        "</thead><tbody>\n",
	        title);

	for (size_t i = 0; i < nrefs; i++) {
		writelog(info, git_reference_shorthand(refs[i].ref), refs[i].commit);
		writearchive(info, git_reference_shorthand(refs[i].ref), refs[i].commit);
		writefiles(info, git_reference_shorthand(refs[i].ref), refs[i].commit);
		writecommitatom(atom, refs[i].commit, git_reference_shorthand(refs[i].ref));

		if (i > 0)
			fprintf(json, ",\n");
		writejsonref(json, info, refs[i].ref, refs[i].commit);

		name    = git_reference_shorthand(refs[i].ref);
		author  = git_commit_author(refs[i].commit);
		summary = git_commit_summary(refs[i].commit);
		git_oid_tostr(oid, sizeof(oid), git_commit_id(refs[i].commit));

		strlcpy(escapename, name, sizeof(escapename));
		for (char* p = escapename; *p; p++)
			if (*p == '/')
				*p = '-';

		hprintf(fp, "<tr><td><b>%y</b>", name);
		fprintf(fp,
		        " <small>at \"<a href=\"commit/%s.html\">%s</a>\"</small>"
		        " <a href=\"%s.html\">[log]</a>"
		        " <a href=\"file/%s/index.html\">[files]</a>"
		        " <a href=\"archive/%s.tar.gz\">[%s.tar.gz]</a>"
		        " <a href=\"archive/%s.zip\">[%s.zip]</a></td><td>",
		        oid, summary, name, name, name, name, name, name);
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

#define ADDREF(refs, nrefs)                                                      \
	{                                                                            \
		if (!(refs = realloc(refs, (nrefs + 1) * sizeof(struct referenceinfo)))) \
			return -1;                                                           \
                                                                                 \
		refs[nrefs].ref    = ref;                                                \
		refs[nrefs].commit = commit;                                             \
		nrefs++;                                                                 \
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
