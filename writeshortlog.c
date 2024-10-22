#include "gitin.h"

#include <git2/commit.h>
#include <git2/revwalk.h>
#include <string.h>
#include <time.h>

#define MAXREFS      64
#define SECONDSINDAY (60 * 60 * 24)
#define DAYSINYEAR   365
#define DAYSINMONTH  28    // at least 28 days

struct authorcount {
	char* name;
	char* email;
	int   count;
};

struct datecount {
	size_t day;
	int    count;
	char   refs[MAXREFS];
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

static void writediagram(FILE* file, struct datecount* datecount, int ndatecount, int bymonth) {
	// Constants for the SVG size and scaling
	const int width        = 1200;
	const int height       = 400;
	const int xpadding     = 20;
	const int point_radius = 3;
	const int textpadding  = 100;
	const int refpadding   = 50;

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
	float y_scale = (float) (height - refpadding - textpadding) / max_commits;

	// Draw the line graph from right to left
	for (int i = 0; i < ndatecount - 1; i++) {
		int x1 = width - xpadding - i * x_scale;
		int y1 = height - datecount[i].count * y_scale - textpadding;
		int x2 = width - xpadding - (i + 1) * x_scale;
		int y2 = height - datecount[i + 1].count * y_scale - textpadding;

		fprintf(
		    file,
		    "  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"#3498db\" stroke-width=\"2\"/>\n",
		    x1, y1, x2, y2);
	}

	// Add vertical labels for months, also right to left
	for (int i = 0; i < ndatecount; i++) {
		int x  = width - xpadding - i * x_scale;    // Reversed X
		int y  = height - textpadding + 10;         // Adjust for spacing below the graph
		int ty = height - datecount[i].count * y_scale - textpadding;

		time_t    secs;
		struct tm time;

		if (datecount[i].count > 0) {
			secs = datecount[i].day * SECONDSINDAY;
			gmtime_r(&secs, &time);

			fprintf(
			    file,
			    "  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">",
			    x - 2, y, x - 2, y);

			if (bymonth)
				fprintf(file, "%s %d", months[time.tm_mon], time.tm_year + 1900);
			else
				fprintf(file, "%d %s %d", time.tm_mday, months[time.tm_mon], time.tm_year + 1900);

			fprintf(file, "</text>\n");

			fprintf(
			    file,
			    "  <text x=\"%d\" y=\"%d\" font-size=\"10px\" text-anchor=\"middle\">%d</text>\n",
			    x, ty - 10, datecount[i].count);
			fprintf(file, "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"#3498db\"/>\n", x, ty,
			        point_radius);
		}
		if (*datecount[i].refs) {
			int y2 = height - datecount[i].count * y_scale - textpadding;
			fprintf(
			    file,
			    "  <line x1=\"%d\" y1=\"%ld\" x2=\"%d\" y2=\"%d\" stroke=\"#000\" stroke-width=\"1\" stroke-dasharray=\"4\"/>\n",
			    x, 20 + 5 * strlen(datecount[i].refs), x, y2 - 30);

			fprintf(
			    file,
			    "  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">%s</text>\n",
			    x - 2, 10, x - 2, 10, datecount[i].refs);
		}
	}

	// SVG footer
	fprintf(file, "</svg>\n");
}

int mergedatecount(struct datecount* datecount, int ndatecount) {
	struct datecount* merged = malloc((ndatecount / DAYSINMONTH + 1) * sizeof(struct datecount));
	if (!merged) {
		fprintf(stderr, "error: unable to allocate memory\n");
		return 0;
	}

	int       merged_index   = 0;
	time_t    first_day_secs = datecount[0].day * SECONDSINDAY;
	struct tm first_day;
	gmtime_r(&first_day_secs, &first_day);

	merged[merged_index].day     = datecount[0].day;    // Initialize with the first day
	merged[merged_index].count   = 0;
	merged[merged_index].refs[0] = '\0';

	for (int i = 0; i < ndatecount; i++) {
		time_t    current_secs = datecount[i].day * SECONDSINDAY;
		struct tm current_day;
		gmtime_r(&current_secs, &current_day);

		if (current_day.tm_mon == first_day.tm_mon && current_day.tm_year == first_day.tm_year) {
			// Same month, accumulate the counts
			merged[merged_index].count += datecount[i].count;
			if (*datecount[i].refs) {
				if (*merged[merged_index].refs) {
					strncat(merged[merged_index].refs, ", ",
					        sizeof(merged[merged_index].refs) - strlen(merged[merged_index].refs) -
					            1);
				}
				strncat(merged[merged_index].refs, datecount[i].refs,
				        sizeof(merged[merged_index].refs) - strlen(merged[merged_index].refs) - 1);
			}
		} else {
			// Move to the next month
			merged_index++;
			merged[merged_index].day   = datecount[i].day;
			merged[merged_index].count = datecount[i].count;
			memcpy(merged[merged_index].refs, datecount[i].refs, MAXREFS);

			first_day = current_day;
		}
	}

	// Copy the merged results back into datecount
	memcpy(datecount, merged, sizeof(struct datecount) * (merged_index + 1));
	free(merged);

	return merged_index + 1;    // Return the number of merged entries (per month)
}

void writeshortlog(FILE* fp, const struct repoinfo* info, git_commit* head) {
	struct authorcount*     authorcount  = NULL;
	struct datecount*       datecount    = NULL;
	int                     nauthorcount = 0, ndatecount = 0, bymonth;
	git_revwalk*            w = NULL;
	git_oid                 id;
	git_commit*             commit = NULL;
	const git_signature*    author;
	size_t                  previous;
	git_reference_iterator* iter = NULL;
	git_reference*          ref  = NULL;
	size_t                  days;

	git_revwalk_new(&w, info->repo);
	git_revwalk_push(w, git_commit_id(head));

	while (!git_revwalk_next(&id, w)) {
		if (git_commit_lookup(&commit, info->repo, &id)) {
			hprintf(stderr, "warn: unable to lookup commit: %gw\n");
			continue;
		}
		if (!(author = git_commit_author(commit)))
			continue;

		days = author->when.time / SECONDSINDAY;

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

		if (ndatecount == 0)
			previous = days + 1;

		// we expect that commits are in chronological order
		if (previous < days)
			continue;

		size_t gap = previous - days;
		for (size_t i = 0; i < gap; i++) {
			// make space for new author
			if (!(datecount = realloc(datecount, (ndatecount + 1) * sizeof(*datecount)))) {
				fprintf(stderr, "error: unable to allocate memory\n");
				exit(100);
			}

			previous--;

			// init new date
			datecount[ndatecount].count   = 0;
			datecount[ndatecount].day     = previous;
			datecount[ndatecount].refs[0] = '\0';
			ndatecount++;
		}

		datecount[ndatecount - 1].count++;

		git_commit_free(commit);
	}

	git_revwalk_free(w);
	w = NULL;

	if (git_reference_iterator_new(&iter, info->repo))
		return;

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

		days = git_commit_time(commit) / SECONDSINDAY;

		for (int i = 0; i < ndatecount; i++) {
			if (datecount[i].day == days) {
				if (*datecount[i].refs)
					strncat(datecount[i].refs, ", ", sizeof(datecount[i].refs));

				if (git_reference_is_tag(ref))
					strncat(datecount[i].refs, "[", sizeof(datecount[i].refs));
				strncat(datecount[i].refs, git_reference_shorthand(ref),
				        sizeof(datecount[i].refs) - 1);
				if (git_reference_is_tag(ref))
					strncat(datecount[i].refs, "]", sizeof(datecount[i].refs));
				break;
			}
		}

		git_reference_free(ref);
		git_commit_free(commit);
		ref = NULL, commit = NULL;
	}
	git_reference_iterator_free(iter);

	if (!authorcount)
		return;

	qsort(authorcount, nauthorcount, sizeof(*authorcount), compareauthor);

	bymonth = ndatecount > DAYSINYEAR;
	if (bymonth)
		ndatecount = mergedatecount(datecount, ndatecount);

	fputs("<h2>Shortlog</h2>", fp);
	fputs("<table><thead>\n<tr><td class=\"num\">Count</td>"
	      "<td class=\"expand\">Author</td>"
	      "<td>E-Mail</td></tr>\n</thead><tbody>\n",
	      fp);

	for (int i = 0; i < nauthorcount; i++) {
		fprintf(fp, "<tr><td>%d</td><td>%s</td><td>%s</td></tr>", authorcount[i].count,
		        authorcount[i].name, authorcount[i].email);
	}

	fputs("</tbody></table>", fp);

	fputs("<h2>Commit Graph</h2>", fp);
	writediagram(fp, datecount, ndatecount, bymonth);

	for (int i = 0; i < nauthorcount; i++) {
		free(authorcount[i].name);
		free(authorcount[i].email);
	}
	free(authorcount);
	free(datecount);
}
