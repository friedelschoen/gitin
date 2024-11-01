#include "hprintf.h"
#include "writer.h"

#include <git2/commit.h>
#include <limits.h>
#include <string.h>

int writerefs(FILE* fp, const struct repoinfo* info, int relpath, git_reference* current) {
	char                 escapename[NAME_MAX], oid[GIT_OID_SHA1_HEXSIZE + 1];
	const char *         name, *summary;
	const git_signature* author;
	git_reference*       ref;
	git_commit*          commit;
	int                  isbranch = 1;

	fprintf(fp, "<h2>Branches</h2><table>"
	            "<thead>\n<tr><td class=\"expand\">Name</td>"
	            "<td>Last commit date</td>"
	            "<td>Author</td>\n</tr>\n"
	            "</thead><tbody>\n");

	for (int i = 0; i < info->nrefs; i++) {
		ref    = info->refs[i].ref;
		commit = info->refs[i].commit;
		name   = git_reference_shorthand(ref);

		if (isbranch && info->refs[i].istag) {
			fputs("</tbody></table>\n", fp);
			fprintf(fp, "<h2>Tags</h2><table>"
			            "<thead>\n<tr><td class=\"expand\">Name</td>"
			            "<td>Last commit date</td>"
			            "<td>Author</td>\n</tr>\n"
			            "</thead><tbody>\n");

			isbranch = 0;
		}

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
		hprintf(fp,
		        " <small>at \"<a href=\"%rcommit/%s.html\">%s</a>\"</small>"
		        " <a href=\"%r%s\">log</a>"
		        " <a href=\"%r%s/files\">files</a>",
		        relpath, oid, summary, relpath, name, relpath, name);

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
