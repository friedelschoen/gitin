#include "gitin.h"

#include <git2/commit.h>
#include <git2/oid.h>
#include <git2/refs.h>
#include <git2/types.h>
#include <limits.h>
#include <string.h>

static int writerefhtml(FILE* fp, struct referenceinfo* refs, size_t nrefs, const char* title,
                        git_reference* current) {
	char                 escapename[NAME_MAX], oid[GIT_OID_SHA1_HEXSIZE + 1];
	const char *         name, *summary;
	const git_signature* author;
	git_reference*       ref;
	git_commit*          commit;

	if (!nrefs)
		return 0;

	fprintf(fp,
	        "<h2>%s</h2><table>"
	        "<thead>\n<tr><td class=\"expand\">Name</td>"
	        "<td>Last commit date</td>"
	        "<td>Author</td>\n</tr>\n"
	        "</thead><tbody>\n",
	        title);

	for (size_t i = 0; i < nrefs; i++) {
		ref    = refs[i].ref;
		commit = refs[i].commit;
		name   = git_reference_shorthand(ref);

		author  = git_commit_author(commit);
		summary = git_commit_summary(commit);
		git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));

		strlcpy(escapename, name, sizeof(escapename));
		for (char* p = escapename; *p; p++)
			if (*p == '/')
				*p = '-';

		fprintf(fp, "<tr><td>");
		// is current
		if (!git_reference_cmp(ref, current))
			hprintf(fp, "<b>%y</b>", name);
		else
			hprintf(fp, "%y", name);
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

int writerefs(FILE* fp, const struct repoinfo* info, git_reference* current) {

	writerefhtml(fp, info->branches, info->nbranches, "Branches", current);

	writerefhtml(fp, info->tags, info->ntags, "Tags", current);

	return 0;
}
