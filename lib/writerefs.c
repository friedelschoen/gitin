#include "hprintf.h"
#include "writer.h"

#include <ctype.h>
#include <git2/commit.h>
#include <limits.h>
#include <string.h>

#define MAXSUMMARY 30

static void writerefheader(FILE* fp, const char* title) {
	fprintf(fp,
	        "<div class=\"ref\">\n<h2>%s</h2>\n<table>"
	        "<thead>\n<tr><td class=\"expand\">Name</td>"
	        "<td>Last commit date</td>"
	        "<td>Author</td>\n</tr>\n"
	        "</thead><tbody>\n",
	        title);
}

static void writereffooter(FILE* fp) {
	fputs("</tbody></table></div>\n", fp);
}

int writerefs(FILE* fp, const struct repoinfo* info, int relpath, git_reference* current) {
	char escapename[NAME_MAX], oid[GIT_OID_SHA1_HEXSIZE + 1], summary[MAXSUMMARY + 2];
	const git_signature* author;
	int                  isbranch = 1;

	writerefheader(fp, "Branches");

	for (int i = 0; i < info->nrefs; i++) {

		git_reference* ref       = info->refs[i].ref;
		git_commit*    commit    = info->refs[i].commit;
		const char*    name      = info->refs[i].refname;
		int            iscurrent = !git_reference_cmp(ref, current);

		if (isbranch && info->refs[i].istag) {
			writereffooter(fp);
			writerefheader(fp, "Tags");
			isbranch = 0;
		}

		author = git_commit_author(commit);
		strlcpy(summary, git_commit_summary(commit), sizeof(summary));
		git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));

		if (strlen(summary) > MAXSUMMARY) {
			for (int j = MAXSUMMARY - 4; j >= 0; j--) {
				if (isspace(summary[j]) && !isspace(summary[j - 1])) {
					j++;
					summary[j++] = '.';
					summary[j++] = '.';
					summary[j++] = '.';
					summary[j]   = '\0';
					break;
				}
			}
		}

		strlcpy(escapename, name, sizeof(escapename));
		for (char* p = escapename; *p; p++)
			if (*p == '/')
				*p = '-';

		if (iscurrent)
			fprintf(fp, "<tr class=\"current-ref\"><td>");
		else
			fprintf(fp, "<tr><td>");
		/* is current */
		hprintf(fp, "<a href=\"%r%s/\">%y</a>", relpath, name, name);
		hprintf(fp, " <small>at \"<a href=\"%rcommit/%s.html\">%s</a>\"</small>", relpath, oid,
		        summary);

		fputs("</td><td>", fp);
		if (author)
			hprintf(fp, "%t", &author->when);
		fputs("</td><td>", fp);
		if (author)
			hprintf(fp, "%y", author->name);
		fputs("</td></tr>\n", fp);
	}

	writereffooter(fp);

	return 0;
}
