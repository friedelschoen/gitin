#include "gitin.h"

#include <git2/commit.h>
#include <git2/revwalk.h>
#include <string.h>
#include <time.h>

#define MONTHDIFFERENCE(year1, month1, year2, month2) ((year1 - year2) * 12 + (month1 - month2))


struct authorcount {
	char* name;
	char* email;
	int   count;
};

struct datecount {
	int year;
	int month;
	int count;
};

static const char* months[] = {
	"January", "February", "March",     "April",   "May",      "Juni",
	"July",    "August",   "September", "October", "November", "December",
};

static int incrementauthor(struct authorcount* authorcount, int nauthorcount,
                           const git_signature* author) {
	for (int i = 0; i < nauthorcount; i++) {
		if (!strcmp(author->name, authorcount[i].name) &&
		    !strcmp(author->email, authorcount[i].email)) {
			authorcount[i].count++;
			return 1;
		}
	}
	return 0;
}

static int compareauthor(const void* leftp, const void* rightp) {
	const struct authorcount* left  = leftp;
	const struct authorcount* right = rightp;

	return right->count - left->count;
}

static void writediagram(FILE* file, struct datecount* datecount, int ndatecount) {
	// Constants for the SVG size and scaling
	const int width        = 1200;
	const int height       = 400;
	const int xpadding     = 20;
	const int ypadding     = 30;
	const int point_radius = 5;
	const int textpadding  = 70;

	int max_commits = 0;

	for (int i = 0; i < ndatecount; i++) {
		if (max_commits < datecount[i].count) {
			max_commits = datecount[i].count;
		}
	}

	// SVG header
	fprintf(
	    file,
	    "<svg id=\"shortlog-graph\" xmlns=\"http://www.w3.org/2000/svg\" viewbox=\"0 0 %d %d\" width=\"100%%\">\n",
	    width, height);

	// Scaling factors for graph
	float x_scale = (float) (width - 2 * xpadding) / (ndatecount - 1);
	float y_scale = (float) (height - 2 * ypadding - textpadding) / max_commits;

	// Draw the line graph from right to left
	for (int i = 0; i < ndatecount - 1; i++) {
		int x1 = width - xpadding - i * x_scale;
		int y1 = height - ypadding - datecount[i].count * y_scale - textpadding;
		int x2 = width - xpadding - (i + 1) * x_scale;
		int y2 = height - ypadding - datecount[i + 1].count * y_scale - textpadding;

		fprintf(
		    file,
		    "  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"blue\" stroke-width=\"2\"/>\n",
		    x1, y1, x2, y2);
	}

	// Add vertical labels for months, also right to left
	for (int i = 0; i < ndatecount; i++) {
		int x  = width - xpadding - i * x_scale;          // Reversed X
		int y  = height - ypadding - textpadding + 10;    // Adjust for spacing below the graph
		int ty = height - ypadding - datecount[i].count * y_scale - textpadding;

		if (datecount[i].count > 0) {
			fprintf(
			    file,
			    "  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">%s %d</text>\n",
			    x - 2, y, x - 2, y, months[datecount[i].month], datecount[i].year + 1900);

			fprintf(
			    file,
			    "  <text x=\"%d\" y=\"%d\" font-size=\"10px\" text-anchor=\"middle\">%d</text>\n",
			    x, ty - 10, datecount[i].count);
			fprintf(file, "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"#3498db\"/>\n", x, ty,
			        point_radius);
		}
	}

	// SVG footer
	fprintf(file, "</svg>\n");
}

void writeshortlog(FILE* fp, const struct repoinfo* info) {
	struct authorcount*  authorcount  = NULL;
	struct datecount*    datecount    = NULL;
	int                  nauthorcount = 0, ndatecount = 0, monthgap;
	git_revwalk*         w = NULL;
	git_oid              id;
	git_commit*          commit = NULL;
	const git_signature* author;
	struct tm            time;
	int                  prev_month, prev_year;

	git_revwalk_new(&w, info->repo);
	git_revwalk_push_head(w);

	while (!git_revwalk_next(&id, w)) {
		if (git_commit_lookup(&commit, info->repo, &id)) {
			hprintf(stderr, "warn: unable to lookup commit: %gw\n");
			continue;
		}
		if (!(author = git_commit_author(commit)))
			continue;

		gmtime_r(&author->when.time, &time);

		if (!incrementauthor(authorcount, nauthorcount, author)) {
			// make space for new author
			if (!(authorcount = realloc(authorcount, (nauthorcount + 1) * sizeof(*authorcount)))) {
				fprintf(stderr, "error: unable to allocate memory\n");
				exit(100);
			}

			// set new author
			authorcount[nauthorcount].count = 1;    // including this commit
			authorcount[nauthorcount].name  = strdup(author->name);
			authorcount[nauthorcount].email = strdup(author->email);
			nauthorcount++;
		}

		if (ndatecount == 0) {
			prev_month = time.tm_mon + 1;
			prev_year  = time.tm_year;
		}

		monthgap = MONTHDIFFERENCE(prev_year, prev_month, time.tm_year, time.tm_mon);

		for (int i = 0; i < monthgap; i++) {
			// make space for new author
			if (!(datecount = realloc(datecount, (ndatecount + 1) * sizeof(*datecount)))) {
				fprintf(stderr, "error: unable to allocate memory\n");
				exit(100);
			}

			prev_month--;
			if (prev_month < 0) {
				prev_month = 11;
				prev_year--;
			}

			// init new date
			datecount[ndatecount].count = 0;
			datecount[ndatecount].month = prev_month;
			datecount[ndatecount].year  = prev_year;
			ndatecount++;
		}

		datecount[ndatecount - 1].count++;
	}

	git_revwalk_free(w);

	if (!authorcount)
		return;

	qsort(authorcount, nauthorcount, sizeof(*authorcount), compareauthor);

	fputs("<h2>Shortlog</h2>", fp);
	fputs("<table><thead>\n<tr><td class=\"num\"><b>Count</b></td>"
	      "<td class=\"expand\"><b>Author</b></td>"
	      "<td><b>E-Mail</b></td></tr>\n</thead><tbody>\n",
	      fp);

	for (int i = 0; i < nauthorcount; i++) {
		fprintf(fp, "<tr><td>%d</td><td>%s</td><td>%s</td></tr>", authorcount[i].count,
		        authorcount[i].name, authorcount[i].email);
	}

	fputs("</tbody></table>", fp);

	fputs("<h2>Commit Graph</h2>", fp);
	writediagram(fp, datecount, ndatecount);

	for (int i = 0; i < nauthorcount; i++) {
		free(authorcount[i].name);
		free(authorcount[i].email);
	}
	free(authorcount);
	free(datecount);
}
