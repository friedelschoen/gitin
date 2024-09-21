#include "commit.h"
#include "common.h"
#include "compat.h"
#include "config.h"
#include "hprintf.h"
#include "parseconfig.h"
#include "writer.h"

#include <err.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>


static int writerefs(FILE* fp, const struct repoinfo* info) {
	struct referenceinfo* ris = NULL;
	struct commitinfo*    ci;
	size_t                count, i, j, refcount;
	const char*           titles[] = { "Branches", "Tags" };
	const char*           ids[]    = { "branches", "tags" };
	const char*           s;

	if (getrefs(&ris, &refcount, info->repo) == -1)
		return -1;

	for (i = 0, j = 0, count = 0; i < refcount; i++) {
		if (j == 0 && git_reference_is_tag(ris[i].ref)) {
			if (count)
				fputs("</tbody></table><br/>\n", fp);
			count = 0;
			j     = 1;
		}

		/* print header if it has an entry (first). */
		if (++count == 1) {
			fprintf(fp,
			        "<h2>%s</h2><table id=\"%s\">"
			        "<thead>\n<tr><td><b>Name</b></td>"
			        "<td><b>Last commit date</b></td>"
			        "<td><b>Author</b></td>\n</tr>\n"
			        "</thead><tbody>\n",
			        titles[j], ids[j]);
		}

		ci = ris[i].ci;
		s  = git_reference_shorthand(ris[i].ref);

		hprintf(fp, "<tr><td>%y</td><td>", s);
		if (ci->author)
			hprintf(fp, "%t", &ci->author->when);
		fputs("</td><td>", fp);
		if (ci->author)
			hprintf(fp, "%y", ci->author->name);
		fputs("</td></tr>\n", fp);
	}
	/* table footer */
	if (count)
		fputs("</tbody></table><br/>\n", fp);

	for (i = 0; i < refcount; i++) {
		commitinfo_free(ris[i].ci);
		git_reference_free(ris[i].ref);
	}
	free(ris);

	return 0;
}

void freeheadfiles(struct repoinfo* info) {
	if (info->headfiles != NULL) {
		// Free each string in the list
		for (int i = 0; i < info->headfileslen; i++) {
			free(info->headfiles[i]);
		}
		// Free the list itself
		free(info->headfiles);
		info->headfiles = NULL;
	}

	info->headfileslen   = 0;
	info->headfilesalloc = 0;
}

void writerepo(FILE* index, const char* repodir) {
	struct repoinfo info;

	git_object*    obj  = NULL;
	const git_oid* head = NULL;
	mode_t         mask;
	FILE*          fp;
	char           path[PATH_MAX];
	char           tmppath[64] = "gitin-cache.XXXXXX", buf[BUFSIZ];
	size_t         n;
	int            i, fd;
	const char*    start;

	memset(&info, 0, sizeof(info));
	info.repodir = repodir;

	info.relpath = 1;
	for (const char* p = repodir + 1; p[1]; p++)
		if (*p == '/')
			info.relpath++;

	snprintf(info.destdir, sizeof(info.destdir), "%s%s", destination, info.repodir);
	normalize_path(info.destdir);

	if (mkdirp(info.destdir) == -1) {
		err(1, "cannot create destdir: %s", info.destdir);
		return;
	}

	if (git_repository_open_ext(&info.repo, info.repodir, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) < 0) {
		err(1, "cannot open repository");
		return;
	}

	/* find HEAD */
	if (!git_revparse_single(&obj, info.repo, "HEAD"))
		head = git_object_id(obj);
	git_object_free(obj);

	start = repodir;
	if (start[0] == '/')
		start++;
	/* use directory name as name */
	strlcpy(info.name, start, sizeof(info.name));
	if (info.name[strlen(info.name) - 1] == '/')
		info.name[strlen(info.name) - 1] = '\0';

	struct configstate state;
	memset(&state, 0, sizeof(state));
	snprintf(path, sizeof(path), "%s/%s", repodir, configfile);
	normalize_path(path);

	if ((fp = fopen(path, "r"))) {
		while (!parseconfig(&state, fp)) {
			if (!strcmp(state.key, "description"))
				strlcpy(info.description, state.value, sizeof(info.description));
			else if (!strcmp(state.key, "url") || !strcmp(state.key, "cloneurl"))
				strlcpy(info.cloneurl, state.value, sizeof(info.cloneurl));
			else
				fprintf(stderr, "warn: ignoring unknown config-key '%s'\n", state.key);
		}
	}

	/* check pinfiles */
	for (i = 0; i < npinfiles && info.pinfileslen < MAXPINS; i++) {
		snprintf(path, sizeof(path), "HEAD:%s", pinfiles[i]);
		if (!git_revparse_single(&obj, info.repo, path) && git_object_type(obj) == GIT_OBJ_BLOB)
			info.pinfiles[info.pinfileslen++] = pinfiles[i];
		git_object_free(obj);
	}

	if (!git_revparse_single(&obj, info.repo, "HEAD:.gitmodules") && git_object_type(obj) == GIT_OBJ_BLOB)
		info.submodules = ".gitmodules";
	git_object_free(obj);

	/* files for HEAD, it must be before writelog, as it also populates headfiles! */
	snprintf(path, sizeof(path), "%s/files.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	fprintf(stderr, "%s\n", path);
	writeheader(fp, &info, 0, "Files", "");
	if (head)
		writefiles(fp, &info, head);
	writefooter(fp);
	checkfileerror(fp, path, 'w');
	fclose(fp);

	/* log for HEAD */
	snprintf(path, sizeof(path), "%s/log.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	fprintf(stderr, "%s\n", path);
	snprintf(path, sizeof(path), "%s/commit", info.destdir);
	mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
	writeheader(fp, &info, 0, "Log", "");
	fputs("<table id=\"log\"><thead>\n<tr><td><b>Date</b></td>"
	      "<td><b>Commit message</b></td>"
	      "<td><b>Author</b></td><td class=\"num\" align=\"right\"><b>Files</b></td>"
	      "<td class=\"num\" align=\"right\"><b>+</b></td>"
	      "<td class=\"num\" align=\"right\"><b>-</b></td></tr>\n</thead><tbody>\n",
	      fp);

	if (commitcache && head) {
		/* read from cache file (does not need to exist) */
		if ((info.rcachefp = fopen(commitcache, "r"))) {
			if (!fgets(info.lastoidstr, sizeof(info.lastoidstr), info.rcachefp))
				errx(1, "%s: no object id", commitcache);
			if (git_oid_fromstr(&info.lastoid, info.lastoidstr))
				errx(1, "%s: invalid object id", commitcache);
		}

		/* write log to (temporary) cache */
		if ((fd = mkstemp(tmppath)) == -1)
			err(1, "mkstemp");
		if (!(info.wcachefp = fdopen(fd, "w")))
			err(1, "fdopen: '%s'", tmppath);
		/* write last commit id (HEAD) */
		git_oid_tostr(buf, sizeof(buf), head);
		fprintf(info.wcachefp, "%s\n", buf);

		writelog(fp, &info, head);

		if (info.rcachefp) {
			/* append previous log to log.html and the new cache */
			while (!feof(info.rcachefp)) {
				n = fread(buf, 1, sizeof(buf), info.rcachefp);
				if (ferror(info.rcachefp))
					break;
				if (fwrite(buf, 1, n, fp) != n || fwrite(buf, 1, n, info.wcachefp) != n)
					break;
			}
			checkfileerror(info.rcachefp, commitcache, 'r');
			fclose(info.rcachefp);
		}
		checkfileerror(info.wcachefp, tmppath, 'w');
		fclose(info.wcachefp);
	} else {
		if (head)
			writelog(fp, &info, head);
	}

	fputs("</tbody></table>", fp);
	writefooter(fp);
	snprintf(path, sizeof(path), "%s/log.html", info.destdir);
	checkfileerror(fp, path, 'w');
	fclose(fp);

	/* summary page with branches and tags */
	snprintf(path, sizeof(path), "%s/refs.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	fprintf(stderr, "%s\n", path);
	writeheader(fp, &info, 0, "References", "");
	writerefs(fp, &info);
	writefooter(fp);
	checkfileerror(fp, path, 'w');
	fclose(fp);

	/* rename new cache file on success */
	if (commitcache && head) {
		if (rename(tmppath, commitcache))
			err(1, "rename: '%s' to '%s'", tmppath, commitcache);
		umask((mask = umask(0)));
		if (chmod(commitcache, (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) & ~mask))
			err(1, "chmod: '%s'", commitcache);
	}

	writeindex(index, &info);
	checkfileerror(index, "index.html", 'w');

	/* cleanup */
	git_repository_free(info.repo);
	freeheadfiles(&info);
}
