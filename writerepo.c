#include "writerepo.h"

#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commitinfo.h"
#include "compat.h"
#include "common.h"
#include "deltainfo.h"
#include "murmur3.h"
#include "refinfo.h"
#include "writeindex.h"
#include "xml.h"

#define MURMUR_SEED 0xCAFE5EED // cafeseed


extern const char *highlightcmd;
extern const char *logoicon;
extern const char *faviconicon;
extern const char *highlightcache;
extern const char *commitcache;
extern const char *destination;
extern const char *licensefiles[];
extern int licensefileslen;
extern const char *readmefiles[];
extern int readmefileslen;
extern const char *indexfile;
extern const char *stylesheet;
extern long long nlogcommits;

static void
writeheader(FILE *fp, const struct repoinfo *info, const char* relpath, const char *title)
{
	fputs("<!DOCTYPE html>\n"
		"<html>\n<head>\n"
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
		"<title>", fp);
	xmlencode(fp, title);
	if (title[0] && info->name[0])
		fputs(" - ", fp);
	xmlencode(fp, info->name);
	if (info->description[0])
		fputs(" - ", fp);
	xmlencode(fp, info->description);
	fprintf(fp, "</title>\n<link rel=\"icon\" type=\"image/svg+xml\" href=\"%s%s\" />\n", relpath, faviconicon);
	fprintf(fp, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s%s\" />\n", relpath, stylesheet);
	fputs("</head>\n<body>\n<table><tr><td>", fp);
	fprintf(fp, "<a href=\"../%s\"><img src=\"%s%s\" alt=\"\" width=\"32\" height=\"32\" /></a>",
	        relpath, relpath, logoicon);
	fputs("</td><td><h1>", fp);
	xmlencode(fp, info->name);
	fputs("</h1><span class=\"desc\">", fp);
	xmlencode(fp, info->description);
	fputs("</span></td></tr>", fp);
	if (info->cloneurl[0]) {
		fputs("<tr class=\"url\"><td></td><td>git clone <a href=\"", fp);
		xmlencode(fp, info->cloneurl); /* not percent-encoded */
		fputs("\">", fp);
		xmlencode(fp, info->cloneurl);
		fputs("</a></td></tr>", fp);
	}
	fputs("<tr><td></td><td>\n", fp);
	fprintf(fp, "<a href=\"%slog.html\">Log</a> | ", relpath);
	fprintf(fp, "<a href=\"%sfiles.html\">Files</a> | ", relpath);
	fprintf(fp, "<a href=\"%srefs.html\">Refs</a>", relpath);
	if (info->submodules)
		fprintf(fp, " | <a href=\"%sfile/%s.html\">Submodules</a>",
		        relpath, info->submodules);
	if (info->readme)
		fprintf(fp, " | <a href=\"%sfile/%s.html\">README</a>",
		        relpath, info->readme);
	if (info->license)
		fprintf(fp, " | <a href=\"%sfile/%s.html\">LICENSE</a>",
		        relpath, info->license);
	fputs("</td></tr></table>\n<hr/>\n<div id=\"content\">\n", fp);
}


static void
writefooter(FILE *fp)
{
	fputs("</div>\n</body>\n</html>\n", fp);
}


static size_t
writeblobhtml(FILE *fp, const git_blob *blob, const char* filename)
{
	static uint32_t highlighthash = 0;

	size_t n = 0, len;
	const char *s = git_blob_rawcontent(blob);

	static unsigned char buffer[512];
	static char cachepath[PATH_MAX];

	FILE* cache;
	pipe_t inpipefd;
	pipe_t outpipefd;
	pid_t process;
	ssize_t readlen;
	int status;
	uint32_t contenthash;
	char* type;

	len = git_blob_rawsize(blob);
	if (len == 0) 
		return 0;

	if (!highlighthash) {
		printf("highlighthash\n");
		highlighthash = murmurhash3(highlightcmd, strlen(highlightcmd), MURMUR_SEED);
	}

	printf("highlighting %s: ", filename);
	fflush(stdout);

	contenthash = murmurhash3(s, len, MURMUR_SEED);
	snprintf(cachepath, sizeof(cachepath), "%s/%x-%x.html", highlightcache, highlighthash, contenthash);
	
	if ((cache = fopen(cachepath, "r"))) {
		n = 0;
		while ((readlen = fread(buffer,1, sizeof(buffer), cache)) > 0) {
			fwrite(buffer, 1, readlen, fp);
			n += readlen;
		}
		fclose(cache);

		printf("cached\n");

		return n;
	}

	if ((type = strrchr(filename, '.')) != NULL)
		type++;

	pipe((int*) &inpipefd);
	pipe((int*) &outpipefd);
	
	if ((process = fork()) == -1) {
		perror("fork(highlight)");
		exit(1);
	} else if (process == 0) {
		// Child
		dup2(outpipefd.read, STDIN_FILENO);
		dup2(inpipefd.write, STDOUT_FILENO);

		//close unused pipe ends
		close(outpipefd.write);
		close(inpipefd.read);

		setenv("filename", filename, 1);
		setenv("type", type, 1);
		execlp("sh", "sh", "-c", highlightcmd, NULL);

		perror("exec(highlight)");
		_exit(1);
	}

 	close(outpipefd.read);
  	close(inpipefd.write);	

	if (write(outpipefd.write, s, len) == -1){
		perror("write(highlight)");
		exit(1);
	}

	close(outpipefd.write);

	cache = fopen(cachepath, "w+");

	n = 0;
	while ((readlen = read(inpipefd.read, buffer, sizeof buffer)) > 0) {
		fwrite(buffer, readlen, 1, fp);
		if (cache) fwrite(buffer, readlen, 1, cache);
		n += readlen;
	}

	close(inpipefd.read);
	if (cache) fclose(cache);

	waitpid(process, &status, 0);

	printf("done\n");

	return n;
}

static void
writecommit(FILE *fp, const char* relpath, struct commitinfo *ci)
{
	fprintf(fp, "<b>commit</b> <a href=\"%scommit/%s.html\">%s</a>\n",
		relpath, ci->oid, ci->oid);

	if (ci->parentoid[0])
		fprintf(fp, "<b>parent</b> <a href=\"%scommit/%s.html\">%s</a>\n",
			relpath, ci->parentoid, ci->parentoid);

	if (ci->author) {
		fputs("<b>Author:</b> ", fp);
		xmlencode(fp, ci->author->name);
		fputs(" &lt;<a href=\"mailto:", fp);
		xmlencode(fp, ci->author->email); /* not percent-encoded */
		fputs("\">", fp);
		xmlencode(fp, ci->author->email);
		fputs("</a>&gt;\n<b>Date:</b>   ", fp);
		printtime(fp, &(ci->author->when));
		putc('\n', fp);
	}
	if (ci->msg) {
		putc('\n', fp);
		xmlencode(fp, ci->msg);
		putc('\n', fp);
	}
}

static void
writediff(FILE *fp, const char* relpath, struct commitinfo *ci)
{
	const git_diff_delta *delta;
	const git_diff_hunk *hunk;
	const git_diff_line *line;
	git_patch *patch;
	size_t nhunks, nhunklines, changed, add, del, total, i, j, k;
	char linestr[80];
	int c;

	writecommit(fp, relpath, ci);

	if (!ci->deltas)
		return;

	if (ci->filecount > 1000   ||
	    ci->ndeltas   > 1000   ||
	    ci->addcount  > 100000 ||
	    ci->delcount  > 100000) {
		fputs("Diff is too large, output suppressed.\n", fp);
		return;
	}

	/* diff stat */
	fputs("<b>Diffstat:</b>\n<table>", fp);
	for (i = 0; i < ci->ndeltas; i++) {
		delta = git_patch_get_delta(ci->deltas[i]->patch);

		switch (delta->status) {
		case GIT_DELTA_ADDED:      c = 'A'; break;
		case GIT_DELTA_COPIED:     c = 'C'; break;
		case GIT_DELTA_DELETED:    c = 'D'; break;
		case GIT_DELTA_MODIFIED:   c = 'M'; break;
		case GIT_DELTA_RENAMED:    c = 'R'; break;
		case GIT_DELTA_TYPECHANGE: c = 'T'; break;
		default:                   c = ' '; break;
		}
		if (c == ' ')
			fprintf(fp, "<tr><td>%c", c);
		else
			fprintf(fp, "<tr><td class=\"%c\">%c", c, c);

		fprintf(fp, "</td><td><a href=\"#h%zu\">", i);
		xmlencode(fp, delta->old_file.path);
		if (strcmp(delta->old_file.path, delta->new_file.path)) {
			fputs(" -&gt; ", fp);
			xmlencode(fp, delta->new_file.path);
		}

		add = ci->deltas[i]->addcount;
		del = ci->deltas[i]->delcount;
		changed = add + del;
		total = sizeof(linestr) - 2;
		if (changed > total) {
			if (add)
				add = ((float)total / changed * add) + 1;
			if (del)
				del = ((float)total / changed * del) + 1;
		}
		memset(&linestr, '+', add);
		memset(&linestr[add], '-', del);

		fprintf(fp, "</a></td><td> | </td><td class=\"num\">%zu</td><td><span class=\"i\">",
		        ci->deltas[i]->addcount + ci->deltas[i]->delcount);
		fwrite(&linestr, 1, add, fp);
		fputs("</span><span class=\"d\">", fp);
		fwrite(&linestr[add], 1, del, fp);
		fputs("</span></td></tr>\n", fp);
	}
	fprintf(fp, "</table></pre><pre>%zu file%s changed, %zu insertion%s(+), %zu deletion%s(-)\n",
		ci->filecount, ci->filecount == 1 ? "" : "s",
	        ci->addcount,  ci->addcount  == 1 ? "" : "s",
	        ci->delcount,  ci->delcount  == 1 ? "" : "s");

	fputs("<hr/>", fp);

	for (i = 0; i < ci->ndeltas; i++) {
		patch = ci->deltas[i]->patch;
		delta = git_patch_get_delta(patch);
		fprintf(fp, "<b>diff --git a/<a id=\"h%zu\" href=\"%sfile/", i, relpath);
		percentencode(fp, delta->old_file.path);
		fputs(".html\">", fp);
		xmlencode(fp, delta->old_file.path);
		fprintf(fp, "</a> b/<a href=\"%sfile/", relpath);
		percentencode(fp, delta->new_file.path);
		fprintf(fp, ".html\">");
		xmlencode(fp, delta->new_file.path);
		fprintf(fp, "</a></b>\n");

		/* check binary data */
		if (delta->flags & GIT_DIFF_FLAG_BINARY) {
			fputs("Binary files differ.\n", fp);
			continue;
		}

		nhunks = git_patch_num_hunks(patch);
		for (j = 0; j < nhunks; j++) {
			if (git_patch_get_hunk(&hunk, &nhunklines, patch, j))
				break;

			fprintf(fp, "<a href=\"#h%zu-%zu\" id=\"h%zu-%zu\" class=\"h\">", i, j, i, j);
			xmlencode(fp, hunk->header);
			fputs("</a>", fp);

			for (k = 0; ; k++) {
				if (git_patch_get_line_in_hunk(&line, patch, j, k))
					break;
				if (line->old_lineno == -1)
					fprintf(fp, "<a href=\"#h%zu-%zu-%zu\" id=\"h%zu-%zu-%zu\" class=\"i\">+",
						i, j, k, i, j, k);
				else if (line->new_lineno == -1)
					fprintf(fp, "<a href=\"#h%zu-%zu-%zu\" id=\"h%zu-%zu-%zu\" class=\"d\">-",
						i, j, k, i, j, k);
				else
					putc(' ', fp);
				xmlencodeline(fp, line->content, line->content_len);
				putc('\n', fp);
				if (line->old_lineno == -1 || line->new_lineno == -1)
					fputs("</a>", fp);
			}
		}
	}
}

static void
writelogline(FILE *fp, const char* relpath, struct commitinfo *ci)
{
	fputs("<tr><td>", fp);
	if (ci->author)
		printtimeshort(fp, &(ci->author->when));
	fputs("</td><td>", fp);
	if (ci->summary) {
		fprintf(fp, "<a href=\"%scommit/%s.html\">", relpath, ci->oid);
		xmlencode(fp, ci->summary);
		fputs("</a>", fp);
	}
	fputs("</td><td>", fp);
	if (ci->author)
		xmlencode(fp, ci->author->name);
	fputs("</td><td class=\"num\" align=\"right\">", fp);
	fprintf(fp, "%zu", ci->filecount);
	fputs("</td><td class=\"num\" align=\"right\">", fp);
	fprintf(fp, "+%zu", ci->addcount);
	fputs("</td><td class=\"num\" align=\"right\">", fp);
	fprintf(fp, "-%zu", ci->delcount);
	fputs("</td></tr>\n", fp);
}

static int
writelog(FILE *fp, const struct repoinfo *info, const git_oid *oid)
{
	struct commitinfo *ci;
	git_revwalk *w = NULL;
	git_oid id;
	char path[PATH_MAX], oidstr[GIT_OID_HEXSZ + 1];
	FILE *fpfile;
	size_t remcommits = 0;
	int r;
	const char* relpath;

	git_revwalk_new(&w, info->repo);
	git_revwalk_push(w, oid);

	while (!git_revwalk_next(&id, w)) {
		relpath = "";

		if (commitcache && !memcmp(&id, &info->lastoid, sizeof(id)))
			break;

		git_oid_tostr(oidstr, sizeof(oidstr), &id);
		r = snprintf(path, sizeof(path), "%s/commit/%s.html", info->destdir, oidstr);
		if (r < 0 || (size_t)r >= sizeof(path))
			errx(1, "path truncated: 'commit/%s.html'", oidstr);
		r = access(path, F_OK);

		/* optimization: if there are no log lines to write and
		   the commit file already exists: skip the diffstat */
		if (!nlogcommits) {
			remcommits++;
			if (!r)
				continue;
		}

		if (!(ci = commitinfo_getbyoid(&id, info->repo)))
			break;
		/* diffstat: for stagit HTML required for the log.html line */
		if (commitinfo_getstats(ci, info->repo) == -1)
			goto err;

		if (nlogcommits != 0) {
			writelogline(fp, relpath, ci);
			if (nlogcommits > 0)
				nlogcommits--;
		}

		if (commitcache)
			writelogline(info->wcachefp, relpath, ci);

		/* check if file exists if so skip it */
		if (r) {
			relpath = "../";
			if (!(fpfile = fopen(path, "w")))
				err(1, "fopen: '%s'", path);
			writeheader(fpfile, info, relpath, ci->summary);
			fputs("<pre>", fpfile);
			writediff(fpfile, relpath, ci);
			fputs("</pre>\n", fpfile);
			writefooter(fpfile);
			checkfileerror(fpfile, path, 'w');
			fclose(fpfile);
		}
err:
		commitinfo_free(ci);
	}
	git_revwalk_free(w);

	if (nlogcommits == 0 && remcommits != 0) {
		fprintf(fp, "<tr><td></td><td colspan=\"5\">"
		        "%zu more commits remaining, fetch the repository"
		        "</td></tr>\n", remcommits);
	}

	relpath = "";

	return 0;
}

static size_t
writeblob(const struct repoinfo *info, const char* relpath, git_object *obj, const char *fpath, const char *filename, const char* staticpath, size_t filesize)
{
	char tmp[PATH_MAX] = "", *d;
	const char *p;
	size_t lc = 0;
	FILE *fp;

	if (strlcpy(tmp, fpath, sizeof(tmp)) >= sizeof(tmp))
		errx(1, "path truncated: '%s'", fpath);
	if (!(d = dirname(tmp)))
		err(1, "dirname");

	if (mkdirp(d))
		return -1;

	for (p = fpath, tmp[0] = '\0'; *p; p++) {
		if (*p == '/' && strlcat(tmp, "../", sizeof(tmp)) >= sizeof(tmp))
			errx(1, "path truncated: '../%s'", tmp);
	}
	relpath = tmp;

	if (!(fp = fopen(fpath, "w")))
		err(1, "fopen: '%s'", fpath);

	writeheader(fp, info, relpath, filename);
	fputs("<p> ", fp);
	xmlencode(fp, filename);
	fprintf(fp, " (%zuB) <a href='%s%s'>download</a>", filesize, relpath, staticpath);
	fputs("</p><hr/>", fp);

	if (git_blob_is_binary((git_blob *)obj))
		fputs("<p>Binary file.</p>\n", fp);
	else
		lc = writeblobhtml(fp, (git_blob *)obj, filename);

	writefooter(fp);
	checkfileerror(fp, fpath, 'w');
	fclose(fp);

	relpath = "";

	return lc;
}

static const char *
filemode(git_filemode_t m)
{
	static char mode[11];

	memset(mode, '-', sizeof(mode) - 1);
	mode[10] = '\0';

	if (S_ISREG(m))
		mode[0] = '-';
	else if (S_ISBLK(m))
		mode[0] = 'b';
	else if (S_ISCHR(m))
		mode[0] = 'c';
	else if (S_ISDIR(m))
		mode[0] = 'd';
	else if (S_ISFIFO(m))
		mode[0] = 'p';
	else if (S_ISLNK(m))
		mode[0] = 'l';
	else if (S_ISSOCK(m))
		mode[0] = 's';
	else
		mode[0] = '?';

	if (m & S_IRUSR) mode[1] = 'r';
	if (m & S_IWUSR) mode[2] = 'w';
	if (m & S_IXUSR) mode[3] = 'x';
	if (m & S_IRGRP) mode[4] = 'r';
	if (m & S_IWGRP) mode[5] = 'w';
	if (m & S_IXGRP) mode[6] = 'x';
	if (m & S_IROTH) mode[7] = 'r';
	if (m & S_IWOTH) mode[8] = 'w';
	if (m & S_IXOTH) mode[9] = 'x';

	if (m & S_ISUID) mode[3] = (mode[3] == 'x') ? 's' : 'S';
	if (m & S_ISGID) mode[6] = (mode[6] == 'x') ? 's' : 'S';
	if (m & S_ISVTX) mode[9] = (mode[9] == 'x') ? 't' : 'T';

	return mode;
}

static void 
writefile(git_object *obj, const char *fpath, size_t filesize) {
	char tmp[PATH_MAX] = "", *d;
	const char *p;
	FILE *fp;

	if (strlcpy(tmp, fpath, sizeof(tmp)) >= sizeof(tmp))
		errx(1, "path truncated: '%s'", fpath);
	if (!(d = dirname(tmp)))
		err(1, "dirname");
	if (mkdirp(d))
		return;

	for (p = fpath, tmp[0] = '\0'; *p; p++) {
		if (*p == '/' && strlcat(tmp, "../", sizeof(tmp)) >= sizeof(tmp))
			errx(1, "path truncated: '../%s'", tmp);
	}

	if (!(fp = fopen(fpath, "w")))
		err(1, "fopen: '%s'", fpath);

	fwrite(git_blob_rawcontent((const git_blob*) obj), filesize, 1, fp);
	checkfileerror(fp, fpath, 'w');
	fclose(fp);
}

static void
unhide_filename(char* path) {
	for (char* chr = path; *chr; chr++) {
		if (*chr == '.' && (chr == path || chr[-1] == '/'))
			*chr = '-';
	}
}

static int
writefilestree(FILE *fp, const struct repoinfo *info, const char* relpath, git_tree *tree, const char *path)
{
	const git_tree_entry *entry = NULL;
	git_object *obj = NULL;
	const char *entryname;
	char filepath[PATH_MAX], entrypath[PATH_MAX], staticpath[PATH_MAX], oid[8];
	size_t count, i, lc, filesize;
	int r, ret;

	count = git_tree_entrycount(tree);
	for (i = 0; i < count; i++) {
		if (!(entry = git_tree_entry_byindex(tree, i)) ||
		    !(entryname = git_tree_entry_name(entry)))
			return -1;
		snprintf(entrypath, sizeof(entrypath), "%s/%s", path,entryname);

		r = snprintf(filepath, sizeof(filepath), "%s/file/%s.html", info->destdir, entrypath);
		if (r < 0 || (size_t)r >= sizeof(filepath))
			errx(1, "path truncated: '%s/file/%s.html'", info->destdir, entrypath);

		r = snprintf(staticpath, sizeof(staticpath), "%s/static/%s", info->destdir, entrypath);
		if (r < 0 || (size_t)r >= sizeof(staticpath))
			errx(1, "path truncated: '%s/static/%s'", info->destdir, entrypath);

		unhide_filename(filepath);
		unhide_filename(staticpath);

		if (!git_tree_entry_to_object(&obj, info->repo, entry)) {
			switch (git_object_type(obj)) {
			case GIT_OBJ_BLOB:
				break;
			case GIT_OBJ_TREE:
				/* NOTE: recurses */
				ret = writefilestree(fp, info, relpath, (git_tree *)obj, entrypath);
				git_object_free(obj);
				if (ret)
					return ret;
				continue;
			default:
				git_object_free(obj);
				continue;
			}

			filesize = git_blob_rawsize((git_blob *)obj);
			lc = writeblob(info, relpath, obj, filepath, entryname, staticpath, filesize);

			writefile(obj, staticpath, filesize);

			r = snprintf(filepath, sizeof(filepath), "file/%s.html", entrypath);
			if (r < 0 || (size_t)r >= sizeof(filepath))
				errx(1, "path truncated: 'file/%s.html'", entrypath);

			r = snprintf(staticpath, sizeof(staticpath), "static/%s", entrypath);
			if (r < 0 || (size_t)r >= sizeof(staticpath))
				errx(1, "path truncated: 'static/%s'", entrypath);

			unhide_filename(filepath);
			unhide_filename(staticpath);

			fputs("<tr><td>", fp);
			fputs(filemode(git_tree_entry_filemode(entry)), fp);
			fprintf(fp, "</td><td><a href=\"%s", relpath);
			percentencode(fp, filepath);
			fputs("\">", fp);
			xmlencode(fp, entrypath);
			fputs("</a></td><td class=\"num\" align=\"right\">", fp);
			if (lc > 0)
				fprintf(fp, "%zuL", lc);
			else
				fprintf(fp, "%zuB", filesize);
			fputs("</td></tr>\n", fp);
			git_object_free(obj);
		} else if (git_tree_entry_type(entry) == GIT_OBJ_COMMIT) {
			/* commit object in tree is a submodule */
			fprintf(fp, "<tr><td>m---------</td><td><a href=\"%sfile/-gitmodules.html\">",
				relpath);
			xmlencode(fp, entrypath);
			fputs("</a> @ ", fp);
			git_oid_tostr(oid, sizeof(oid), git_tree_entry_id(entry));
			xmlencode(fp, oid);
			fputs("</td><td class=\"num\" align=\"right\"></td></tr>\n", fp);
		}
	}

	return 0;
}

static int
writefiles(FILE *fp, const struct repoinfo *info, const char* relpath, const git_oid *id)
{
	git_tree *tree = NULL;
	git_commit *commit = NULL;
	int ret = -1;

	fputs("<table id=\"files\"><thead>\n<tr>"
	      "<td><b>Mode</b></td><td><b>Name</b></td>"
	      "<td class=\"num\" align=\"right\"><b>Size</b></td>"
	      "</tr>\n</thead><tbody>\n", fp);

	if (!git_commit_lookup(&commit, info->repo, id) &&
	    !git_commit_tree(&tree, commit))
		ret = writefilestree(fp, info, relpath, tree, "");

	fputs("</tbody></table>", fp);

	git_commit_free(commit);
	git_tree_free(tree);

	return ret;
}

static int
writerefs(FILE *fp, const struct repoinfo *info)
{
	struct referenceinfo *ris = NULL;
	struct commitinfo *ci;
	size_t count, i, j, refcount;
	const char *titles[] = { "Branches", "Tags" };
	const char *ids[] = { "branches", "tags" };
	const char *s;

	if (getrefs(&ris, &refcount, info->repo) == -1)
		return -1;

	for (i = 0, j = 0, count = 0; i < refcount; i++) {
		if (j == 0 && git_reference_is_tag(ris[i].ref)) {
			if (count)
				fputs("</tbody></table><br/>\n", fp);
			count = 0;
			j = 1;
		}

		/* print header if it has an entry (first). */
		if (++count == 1) {
			fprintf(fp, "<h2>%s</h2><table id=\"%s\">"
		                "<thead>\n<tr><td><b>Name</b></td>"
			        "<td><b>Last commit date</b></td>"
			        "<td><b>Author</b></td>\n</tr>\n"
			        "</thead><tbody>\n",
			         titles[j], ids[j]);
		}

		ci = ris[i].ci;
		s = git_reference_shorthand(ris[i].ref);

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

void
writerepo(FILE *index, const char* repodir) {
	static struct repoinfo info;

	git_object *obj = NULL;
	const git_oid *head = NULL;
	mode_t mask;
	FILE *fp, *fpread;
	char path[PATH_MAX], repodirabs[PATH_MAX + 1];
	char tmppath[64] = "stagit-cache.XXXXXX", buf[BUFSIZ];
	size_t n;
	int i, fd;

	memset(&info, 0, sizeof(info));
	info.repodir = repodir;
		
	snprintf(info.destdir, sizeof(info.destdir), "%s/%s/", info.repodir, destination);

	printf("-> %s\n", info.repodir);

	if (!realpath(info.repodir, repodirabs))
		err(1, "realpath");

	if (git_repository_open_ext(&info.repo, info.repodir,
		GIT_REPOSITORY_OPEN_NO_SEARCH, NULL) < 0) {
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

	/* check LICENSE */
	for (i = 0; i < licensefileslen && !info.license; i++) {
		if (!git_revparse_single(&obj, info.repo, licensefiles[i]) &&
			git_object_type(obj) == GIT_OBJ_BLOB)
			info.license = licensefiles[i] + strlen("HEAD:");
		git_object_free(obj);
	}

	/* check README */
	for (i = 0; i < readmefileslen && !info.readme; i++) {
		if (!git_revparse_single(&obj, info.repo, readmefiles[i]) &&
			git_object_type(obj) == GIT_OBJ_BLOB)
			info.readme = readmefiles[i] + strlen("HEAD:");
		git_object_free(obj);
	}

	if (!git_revparse_single(&obj, info.repo, "HEAD:.gitmodules") &&
		git_object_type(obj) == GIT_OBJ_BLOB)
		info.submodules = ".gitmodules";
	git_object_free(obj);

	/* log for HEAD */
	snprintf(path, sizeof(path), "%s/log.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	snprintf(path, sizeof(path), "%s/commit", info.destdir);
	mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
	writeheader(fp, &info, "", "Log");
	fputs("<table id=\"log\"><thead>\n<tr><td><b>Date</b></td>"
		"<td><b>Commit message</b></td>"
		"<td><b>Author</b></td><td class=\"num\" align=\"right\"><b>Files</b></td>"
		"<td class=\"num\" align=\"right\"><b>+</b></td>"
		"<td class=\"num\" align=\"right\"><b>-</b></td></tr>\n</thead><tbody>\n", fp);

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
				if (fwrite(buf, 1, n, fp) != n ||
					fwrite(buf, 1, n, info.wcachefp) != n)
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
	writeheader(fp, &info, "", "Files");
	if (head)
		writefiles(fp, &info, "", head);
	writefooter(fp);
	checkfileerror(fp, path, 'w');
	fclose(fp);

	/* summary page with branches and tags */
	snprintf(path, sizeof(path), "%s/refs.html", info.destdir);
	if (!(fp = fopen(path, "w")))
		err(1, "fopen: '%s'", path);
	writeheader(fp, &info, "", "Refs");
	writerefs(fp, &info);
	writefooter(fp);
	checkfileerror(fp, path, 'w');
	fclose(fp);

	/* rename new cache file on success */
	if (commitcache && head) {
		if (rename(tmppath, commitcache))
			err(1, "rename: '%s' to '%s'", tmppath, commitcache);
		umask((mask = umask(0)));
		if (chmod(commitcache,
			(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) & ~mask))
			err(1, "chmod: '%s'", commitcache);
	}

	writelog_index(index, &info);
	checkfileerror(index, indexfile, 'w');

	/* cleanup */
	git_repository_free(info.repo);
}
