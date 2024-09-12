#include "commit.h"
#include "common.h"
#include "config.h"
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

		fputs("<tr><td>", fp);
		xmlencode(fp, s);
		fputs("</td><td>", fp);
		if (ci->author)
			printtimeshort(fp, &(ci->author->when));
		fputs("</td><td>", fp);
		if (ci->author)
			xmlencode(fp, ci->author->name);
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

void writerepo(FILE* index, const char* repodir) {
	static struct repoinfo info;

	git_object*    obj  = NULL;
	const git_oid* head = NULL;
	mode_t         mask;
	FILE *         fp, *fpread;
	char           path[PATH_MAX], repodirabs[PATH_MAX + 1];
	char           tmppath[64] = "gitin-cache.XXXXXX", buf[BUFSIZ];
	size_t         n;
	int            i, fd;

	memset(&info, 0, sizeof(info));
	info.repodir = repodir;

	snprintf(info.destdir, sizeof(info.destdir), "%s/%s/", info.repodir, destination);

	printf("-> %s\n", info.repodir);

	if (!realpath(info.repodir, repodirabs))
		err(1, "realpath");

	if (git_repository_open_ext(&info.repo, info.repodir, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) < 0) {
		err(1, "cannot open repository");
		return;
	}

	/* find HEAD */
	if (!git_revparse_single(&obj, info.repo, "HEAD"))
		head = git_object_id(obj);
	git_object_free(obj);

	/* use directory name as name */
	if ((info.name = strrchr(repodirabs, '/')))
		info.name++;
	else
		info.name = "";

	snprintf(path, sizeof(path), "%s/description", info.repodir);
	if ((fpread = fopen(path, "r"))) {
		if (!fgets(info.description, sizeof(info.description), fpread))
			info.description[0] = '\0';
		checkfileerror(fpread, path, 'r');
		fclose(fpread);
	}

	snprintf(path, sizeof(path), "%s/url", info.repodir);
	if ((fpread = fopen(path, "r"))) {
		if (!fgets(info.cloneurl, sizeof(info.cloneurl), fpread))
			info.cloneurl[0] = '\0';
		checkfileerror(fpread, path, 'r');
		fclose(fpread);
		info.cloneurl[strcspn(info.cloneurl, "\n")] = '\0';
	}

	/* check pinfiles */
	for (i = 0; i < pinfileslen && info.pinfileslen < MAXPINS; i++) {
		snprintf(path, sizeof(path), "HEAD:%s", pinfiles[i]);
		if (!git_revparse_single(&obj, info.repo, path) && git_object_type(obj) == GIT_OBJ_BLOB)
			info.pinfiles[info.pinfileslen++] = pinfiles[i];
		git_object_free(obj);
	}

	if (!git_revparse_single(&obj, info.repo, "HEAD:.gitmodules") && git_object_type(obj) == GIT_OBJ_BLOB)
		info.submodules = ".gitmodules";
	git_object_free(obj);

	/* log for HEAD */
	snprintf(path, sizeof(path), "%s/log.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	snprintf(path, sizeof(path), "%s/commit", info.destdir);
	mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
	writeheader(fp, &info, "../", "Log", "");
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

	/* files for HEAD */
	snprintf(path, sizeof(path), "%s/files.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	writeheader(fp, &info, "../", "Files", "");
	if (head)
		writefiles(fp, &info, "", head);
	writefooter(fp);
	checkfileerror(fp, path, 'w');
	fclose(fp);

	/* summary page with branches and tags */
	snprintf(path, sizeof(path), "%s/refs.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	writeheader(fp, &info, "../", "References", "");
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
	checkfileerror(index, indexfile, 'w');

	/* cleanup */
	git_repository_free(info.repo);
}
