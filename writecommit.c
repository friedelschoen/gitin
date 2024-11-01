#include "common.h"
#include "hprintf.h"
#include "writer.h"

#include <assert.h>
#include <ctype.h>
#include <git2/commit.h>
#include <string.h>


/* Escape characters below as HTML 2.0 / XML 1.0, ignore printing '\r', '\n' */
static void xmlencodeline(FILE* fp, const char* s, size_t len) {
	size_t        i = 0;
	unsigned char c;
	unsigned int  codepoint;

	while (i < len && *s) {
		c = (unsigned char) *s;

		/* Handle ASCII characters */
		if (c < 0x80) {
			switch (c) {
				case '<':
					fputs("&lt;", fp);
					break;
				case '>':
					fputs("&gt;", fp);
					break;
				case '\'':
					fputs("&#39;", fp);
					break;
				case '&':
					fputs("&amp;", fp);
					break;
				case '"':
					fputs("&quot;", fp);
					break;
				case 0x0B:
				case 0x0C:
					break;
				case '\r':
				case '\n':
					break; /* ignore LF */
				default:
					if (isprint(c) || isblank(c))
						putc(c, fp);
					else
						fprintf(fp, "&#%d;", c);
			}
			s++;
			i++;
		}
		/* Handle multi-byte UTF-8 sequences */
		else if (c < 0xC0) {
			/* Invalid continuation byte at start, print as is */
			fprintf(fp, "&#%d;", c);
			s++;
			i++;
		} else {
			/* Decode UTF-8 sequence */
			const unsigned char* start     = (unsigned char*) s;
			int                  remaining = 0;

			if (c < 0xE0) {
				/* 2-byte sequence */
				remaining = 1;
				codepoint = c & 0x1F;
			} else if (c < 0xF0) {
				/* 3-byte sequence */
				remaining = 2;
				codepoint = c & 0x0F;
			} else if (c < 0xF8) {
				/* 4-byte sequence */
				remaining = 3;
				codepoint = c & 0x07;
			} else {
				/* Invalid start byte, print as is */
				fprintf(fp, "&#%d;", c);
				s++;
				i++;
				continue;
			}

			/* Process continuation bytes */
			while (remaining-- && *(++s)) {
				c = (unsigned char) *s;
				if ((c & 0xC0) != 0x80) {
					/* Invalid continuation byte, print original bytes as is */
					while (start <= (unsigned char*) s) {
						fprintf(fp, "&#%d;", *start++);
					}
					s++;
					i++;
					break;
				}
				codepoint = (codepoint << 6) | (c & 0x3F);
			}

			if (remaining < 0) {
				/* Successfully decoded UTF-8 character, output as numeric reference */
				fprintf(fp, "&#%u;", codepoint);
				s++;
				i++;
			}
		}
	}
}

static int hasheadfile(const struct repoinfo* info, const char* filename) {
	for (int i = 0; i < info->headfileslen; i++) {
		if (strcmp(info->headfiles[i], filename) == 0) {
			return 1;
		}
	}
	return 0;
}

void writecommit(FILE* fp, const struct repoinfo* info, git_commit* commit,
                 const struct commitinfo* ci, int parentlink, const char* refname) {
	const git_diff_delta* delta;
	const git_diff_hunk*  hunk;
	const git_diff_line*  line;
	git_patch*            patch;
	size_t                nhunks, nhunklines, changed, add, del, total, i, j, k;
	char                  linestr[80];
	int                   c;
	char                  oid[GIT_OID_SHA1_HEXSIZE + 1], parentoid[GIT_OID_SHA1_HEXSIZE + 1];
	const git_signature*  author  = git_commit_author(commit);
	const char*           msg     = git_commit_message(commit);
	const char*           summary = git_commit_summary(commit);

	git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));
	git_oid_tostr(parentoid, sizeof(parentoid), git_commit_parent_id(commit, 0));

	writeheader(fp, info, 1, info->name, "%y in %s", summary, refname);
	fputs("<pre>", fp);

	hprintf(fp, "<b>commit</b> <a href=\"%s.html\">%s</a>\n", oid, oid);

	if (*parentoid) {
		if (parentlink)
			hprintf(fp, "<b>parent</b> <a href=\"%s.html\">%s</a>\n", parentoid, parentoid);
		else
			hprintf(fp, "<b>parent</b> %s\n", parentoid);
	}
	if (author)
		hprintf(fp, "<b>Author:</b> %y &lt;<a href=\"mailto:%y\">%y</a>&gt;\n<b>Date:</b>   %T\n",
		        author->name, author->email, author->email, &author->when);

	if (msg)
		hprintf(fp, "\n%y\n", msg);

	if (!ci->deltas)
		return;

	if (ci->ndeltas > 1000 || ci->addcount > 100000 || ci->delcount > 100000) {
		fputs("Diff is too large, output suppressed.\n", fp);
		return;
	}

	/* diff stat */
	fputs("<b>Diffstat:</b>\n<table>", fp);
	for (i = 0; i < ci->ndeltas; i++) {
		assert(ci->deltas[i].patch);
		delta = git_patch_get_delta(ci->deltas[i].patch);

		switch (delta->status) {
			case GIT_DELTA_ADDED:
				c = 'A';
				break;
			case GIT_DELTA_COPIED:
				c = 'C';
				break;
			case GIT_DELTA_DELETED:
				c = 'D';
				break;
			case GIT_DELTA_MODIFIED:
				c = 'M';
				break;
			case GIT_DELTA_RENAMED:
				c = 'R';
				break;
			case GIT_DELTA_TYPECHANGE:
				c = 'T';
				break;
			default:
				c = ' ';
				break;
		}
		if (c == ' ')
			fprintf(fp, "<tr><td>%c", c);
		else
			fprintf(fp, "<tr><td class=\"%c\">%c", c, c);

		hprintf(fp, "</td><td><a href=\"#h%zu\">%y", i, delta->old_file.path);
		if (strcmp(delta->old_file.path, delta->new_file.path))
			hprintf(fp, " -&gt; \n%y", delta->new_file.path);

		add     = ci->deltas[i].addcount;
		del     = ci->deltas[i].delcount;
		changed = add + del;
		total   = sizeof(linestr) - 2;
		if (changed > total) {
			if (add)
				add = ((double) total / changed * add) + 1;
			if (del)
				del = ((double) total / changed * del) + 1;
		}
		memset(&linestr, '+', add);
		memset(&linestr[add], '-', del);

		fprintf(fp, "</a></td><td class=\"num\">%zu</td><td class=\"expand\"><span class=\"i\">",
		        ci->deltas[i].addcount + ci->deltas[i].delcount);
		fwrite(&linestr, 1, add, fp);
		fputs("</span><span class=\"d\">", fp);
		fwrite(&linestr[add], 1, del, fp);
		fputs("</span></td></tr>\n", fp);
	}
	fprintf(fp, "</table></pre><pre>%zu file%s changed, %zu insertion%s(+), %zu deletion%s(-)\n",
	        ci->ndeltas, ci->ndeltas == 1 ? "" : "s", ci->addcount, ci->addcount == 1 ? "" : "s",
	        ci->delcount, ci->delcount == 1 ? "" : "s");

	fputs("<hr/>", fp);

	for (i = 0; i < ci->ndeltas; i++) {
		patch = ci->deltas[i].patch;
		delta = git_patch_get_delta(patch);

		if (hasheadfile(info, delta->old_file.path))
			hprintf(fp, "<b>diff --git a/<a id=\"h%zu\" href=\"../file/%h.html\">%y</a> ", i,
			        delta->old_file.path, delta->old_file.path);
		else
			hprintf(fp, "<b>diff --git a/%y ", delta->old_file.path);

		if (hasheadfile(info, delta->new_file.path))
			hprintf(fp, "b/<a href=\"../file/%h.html\">%y</a></b>\n", delta->new_file.path,
			        delta->new_file.path);
		else
			hprintf(fp, "b/%y</b>\n", delta->new_file.path);

		if (delta->flags & GIT_DIFF_FLAG_BINARY) {
			fputs("Binary files differ.\n", fp);
			continue;
		}

		nhunks = git_patch_num_hunks(patch);
		for (j = 0; j < nhunks; j++) {
			if (git_patch_get_hunk(&hunk, &nhunklines, patch, j))
				break;

			hprintf(fp, "<a href=\"#h%zu-%zu\" id=\"h%zu-%zu\" class=\"h\">%y</a>\n", i, j, i, j,
			        hunk->header);

			for (k = 0;; k++) {
				if (git_patch_get_line_in_hunk(&line, patch, j, k))
					break;
				if (line->old_lineno == -1)
					fprintf(fp, "<a href=\"#h%zu-%zu-%zu\" id=\"h%zu-%zu-%zu\" class=\"i\">+", i, j,
					        k, i, j, k);
				else if (line->new_lineno == -1)
					fprintf(fp, "<a href=\"#h%zu-%zu-%zu\" id=\"h%zu-%zu-%zu\" class=\"d\">-", i, j,
					        k, i, j, k);
				else
					putc(' ', fp);
				xmlencodeline(fp, line->content, line->content_len);
				putc('\n', fp);
				if (line->old_lineno == -1 || line->new_lineno == -1)
					fputs("</a>", fp);
			}
		}
	}

	fputs("</pre>\n", fp);
	writefooter(fp);
}
