#include "gitin.h"

#include <git2/commit.h>
#include <git2/refs.h>
#include <git2/revwalk.h>
#include <git2/types.h>
#include <string.h>
#include <time.h>

#define MAXREFS      64
#define SECONDSINDAY (60 * 60 * 24)
#define DAYSINYEAR   365


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

struct loginfo {
	struct authorcount* authorcount;
	int                 nauthorcount;

	struct datecount* datecount;
	int               ndatecount;
	int               bymonth;
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

static void writediagram(FILE* file, struct loginfo* li) {
	// Constants for the SVG size and scaling
	const int width  = 1200;
	const int height = 500;

	const int padding_bottom = 100;
	const int padding_top    = 100;
	const int padding_left   = 20;
	const int padding_right  = 20;

	const int point_radius = 3;
	const int char_width   = 5;

	const char* color = "#3498db";


	int max_commits = 0;
	for (int i = 0; i < li->ndatecount; i++) {
		if (max_commits < li->datecount[i].count) {
			max_commits = li->datecount[i].count;
		}
	}

	// SVG header
	fprintf(
	    file,
	    "<svg id=\"shortlog-graph\" xmlns=\"http://www.w3.org/2000/svg\" viewbox=\"0 0 %d %d\" width=\"100%%\">\n",
	    width, height);

	// Scaling factors for graph
	double x_scale = (double) (width - padding_left - padding_right) / (li->ndatecount - 1);
	double y_scale = (double) (height - padding_top - padding_bottom) / max_commits;

	// Draw the line graph from right to left
	for (int i = 0; i < li->ndatecount - 1; i++) {
		int x1 = width - padding_right - i * x_scale;
		int y1 = height - padding_bottom - li->datecount[i].count * y_scale;
		int x2 = width - padding_right - (i + 1) * x_scale;
		int y2 = height - padding_bottom - li->datecount[i + 1].count * y_scale;

		fprintf(
		    file,
		    "  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"%s\" stroke-width=\"2\"/>\n",
		    x1, y1, x2, y2, color);
	}

	// Add vertical labels for months, also right to left
	for (int i = 0; i < li->ndatecount; i++) {
		int x  = width - padding_right - i * x_scale;    // Reversed X
		int y  = height - padding_bottom + 10;           // Adjust for spacing below the graph
		int ty = height - padding_bottom - li->datecount[i].count * y_scale;

		time_t    secs;
		struct tm time;

		if (li->datecount[i].count > 0) {
			secs = li->datecount[i].day * SECONDSINDAY;
			gmtime_r(&secs, &time);

			fprintf(
			    file,
			    "  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">",
			    x - 2, y, x - 2, y);

			if (li->bymonth)
				fprintf(file, "%s %d", months[time.tm_mon], time.tm_year + 1900);
			else
				fprintf(file, "%d %s %d", time.tm_mday, months[time.tm_mon], time.tm_year + 1900);

			fprintf(file, "</text>\n");

			fprintf(
			    file,
			    "  <text x=\"%d\" y=\"%d\" font-size=\"10px\" text-anchor=\"middle\">%d</text>\n",
			    x, ty - 10, li->datecount[i].count);
			fprintf(file, "  <circle cx=\"%d\" cy=\"%d\" r=\"%d\" fill=\"%s\"/>\n", x, ty,
			        point_radius, color);
		}
		if (li->datecount[i].refs[0]) {
			int y2 = height - li->datecount[i].count * y_scale - padding_bottom;
			fprintf(
			    file,
			    "  <line x1=\"%d\" y1=\"%ld\" x2=\"%d\" y2=\"%d\" stroke=\"#000\" stroke-width=\"1\" stroke-dasharray=\"4\"/>\n",
			    x, 20 + char_width * strlen(li->datecount[i].refs), x, y2 - 30);

			fprintf(
			    file,
			    "  <text x=\"%d\" y=\"%d\" font-size=\"8px\" text-anchor=\"start\" transform=\"rotate(90 %d,%d)\">%s</text>\n",
			    x - 2, 10, x - 2, 10, li->datecount[i].refs);
		}
	}

	// SVG footer
	fprintf(file, "</svg>\n");
}

static void mergedatecount(struct loginfo* li) {
	// Initialize the first month with the first day
	int       writeptr = 0;
	time_t    first_day_secs, current_secs;
	struct tm first_day, current_day;

	if (li->ndatecount == 0)
		return;

	li->bymonth = 1;

	first_day_secs = li->datecount[0].day * SECONDSINDAY;
	gmtime_r(&first_day_secs, &first_day);

	// Iterate through the rest of datecount
	for (int readptr = 1; readptr < li->ndatecount; readptr++) {
		current_secs = li->datecount[readptr].day * SECONDSINDAY;
		gmtime_r(&current_secs, &current_day);

		if (current_day.tm_mon == first_day.tm_mon && current_day.tm_year == first_day.tm_year) {
			// Same month, accumulate counts
			li->datecount[writeptr].count += li->datecount[readptr].count;

			// Append references if present
			if (li->datecount[readptr].refs[0]) {
				if (li->datecount[writeptr].refs[0]) {
					strncat(li->datecount[writeptr].refs, ", ",
					        MAXREFS - strlen(li->datecount[writeptr].refs) - 1);
				}
				strncat(li->datecount[writeptr].refs, li->datecount[readptr].refs,
				        MAXREFS - strlen(li->datecount[writeptr].refs) - 1);
			}
		} else {
			// Move to the next month
			writeptr++;

			// Copy the new month entry to the next position
			li->datecount[writeptr].day   = li->datecount[readptr].day;
			li->datecount[writeptr].count = li->datecount[readptr].count;
			strncpy(li->datecount[writeptr].refs, li->datecount[readptr].refs, MAXREFS - 1);
			li->datecount[writeptr].refs[MAXREFS - 1] = '\0';

			// Update first_day for the new month
			first_day = current_day;
		}
	}

	li->ndatecount = writeptr + 1;    // Return the number of merged entries
}

static void countlog(const struct repoinfo* info, git_commit* head, struct loginfo* li) {
	git_revwalk*         w      = NULL;
	git_commit*          commit = NULL;
	const git_signature* author;
	git_oid              id;
	size_t               days = 0, previous = 0;

	git_revwalk_new(&w, info->repo);
	git_revwalk_push(w, git_commit_id(head));

	while (!git_revwalk_next(&id, w)) {
		if (git_commit_lookup(&commit, info->repo, &id)) {
			hprintf(stderr, "warn: unable to lookup commit: %gw\n");
			continue;
		}
		author = git_commit_author(commit);

		days = author->when.time / SECONDSINDAY;

		if (!incrementauthor(li->authorcount, li->nauthorcount, author)) {
			// make space for new author
			if (!(li->authorcount = realloc(li->authorcount,
			                                (li->nauthorcount + 1) * sizeof(*li->authorcount)))) {
				fprintf(stderr, "error: unable to allocate memory\n");
				exit(100);
			}

			// set new author
			li->authorcount[li->nauthorcount].count = 1;    // including this commit
			li->authorcount[li->nauthorcount].name  = strdup(author->name);
			li->authorcount[li->nauthorcount].email = strdup(author->email);
			li->nauthorcount++;
		}

		if (li->ndatecount == 0)
			previous = days + 1;

		while (previous > days) {
			previous--;

			// make space for new date
			if (!(li->datecount =
			          realloc(li->datecount, (li->ndatecount + 1) * sizeof(*li->datecount)))) {
				fprintf(stderr, "error: unable to allocate memory\n");
				exit(100);
			}

			// init new date
			li->datecount[li->ndatecount].count   = 0;
			li->datecount[li->ndatecount].day     = previous;
			li->datecount[li->ndatecount].refs[0] = '\0';
			li->ndatecount++;
		}

		if (previous == days)
			li->datecount[li->ndatecount - 1].count++;

		git_commit_free(commit);
	}

	git_revwalk_free(w);

	qsort(li->authorcount, li->nauthorcount, sizeof(*li->authorcount), compareauthor);
}

static void addrefcount(const struct repoinfo* info, struct loginfo* li) {
	git_reference_iterator* iter;
	git_commit*             commit;
	git_reference *         ref, *newref;
	size_t                  days;

	git_reference_iterator_new(&iter, info->repo);
	while (!git_reference_next(&ref, iter)) {
		if (!git_reference_is_branch(ref) && !git_reference_is_tag(ref)) {
			git_reference_free(ref);
			ref = NULL;
			continue;
		}

		if (git_reference_resolve(&newref, ref))
			continue;

		git_reference_free(ref);
		ref = newref;

		if (git_reference_peel((git_object**) &commit, ref, GIT_OBJECT_COMMIT))
			continue;

		days = git_commit_time(commit) / SECONDSINDAY;

		for (int i = 0; i < li->ndatecount; i++) {
			if (li->datecount[i].day == days) {
				if (li->datecount[i].refs[0])
					strncat(li->datecount[i].refs, ", ", MAXREFS);

				if (git_reference_is_tag(ref))
					strncat(li->datecount[i].refs, "[", MAXREFS);
				strncat(li->datecount[i].refs, git_reference_shorthand(ref), MAXREFS - 1);
				if (git_reference_is_tag(ref))
					strncat(li->datecount[i].refs, "]", MAXREFS);

				break;
			}
		}

		git_reference_free(ref);
		git_commit_free(commit);
	}
	git_reference_iterator_free(iter);
}

static void writeauthorlog(FILE* fp, struct loginfo* li) {
	fputs("<table><thead>\n<tr><td class=\"num\">Count</td>"
	      "<td class=\"expand\">Author</td>"
	      "<td>E-Mail</td></tr>\n</thead><tbody>\n",
	      fp);

	for (int i = 0; i < li->nauthorcount; i++) {
		fprintf(fp, "<tr><td>%d</td><td>%s</td><td>%s</td></tr>", li->authorcount[i].count,
		        li->authorcount[i].name, li->authorcount[i].email);
	}

	fputs("</tbody></table>", fp);
}

void writeshortlog(FILE* fp, const struct repoinfo* info, git_commit* head) {
	struct loginfo li;

	memset(&li, 0, sizeof(li));
	countlog(info, head, &li);
	addrefcount(info, &li);

	if (!li.authorcount)
		return;

	if (li.ndatecount > DAYSINYEAR)
		mergedatecount(&li);

	fputs("<h2>Shortlog</h2>", fp);
	writeauthorlog(fp, &li);

	fputs("<h2>Commit Graph</h2>", fp);
	writediagram(fp, &li);

	for (int i = 0; i < li.nauthorcount; i++) {
		free(li.authorcount[i].name);
		free(li.authorcount[i].email);
	}
	free(li.authorcount);
	free(li.datecount);
}
