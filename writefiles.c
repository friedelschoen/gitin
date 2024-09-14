#include "common.h"
#include "compat.h"
#include "config.h"
#include "murmur3.h"
#include "writer.h"

#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MURMUR_SEED 0xCAFE5EED    // cafeseed


static void unhide_filename(char* path) {
	for (char* chr = path; *chr; chr++) {
		if (*chr == '.' && (chr == path || chr[-1] == '/') && chr[1] != '/')
			*chr = '-';
	}
}

static const char* filemode(git_filemode_t m) {
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

	if (m & S_IRUSR)
		mode[1] = 'r';
	if (m & S_IWUSR)
		mode[2] = 'w';
	if (m & S_IXUSR)
		mode[3] = 'x';
	if (m & S_IRGRP)
		mode[4] = 'r';
	if (m & S_IWGRP)
		mode[5] = 'w';
	if (m & S_IXGRP)
		mode[6] = 'x';
	if (m & S_IROTH)
		mode[7] = 'r';
	if (m & S_IWOTH)
		mode[8] = 'w';
	if (m & S_IXOTH)
		mode[9] = 'x';

	if (m & S_ISUID)
		mode[3] = (mode[3] == 'x') ? 's' : 'S';
	if (m & S_ISGID)
		mode[6] = (mode[6] == 'x') ? 's' : 'S';
	if (m & S_ISVTX)
		mode[9] = (mode[9] == 'x') ? 't' : 'T';

	return mode;
}

static size_t writeblobhtml(FILE* fp, const git_blob* blob, const char* filename) {
	static uint32_t highlighthash = 0;

	size_t      n = 0, len;
	const char* s = git_blob_rawcontent(blob);

	static unsigned char buffer[512];
	static char          cachepath[PATH_MAX];

	FILE*    cache;
	pipe_t   inpipefd;
	pipe_t   outpipefd;
	pid_t    process;
	ssize_t  readlen;
	int      status;
	uint32_t contenthash;
	char*    type;

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
		while ((readlen = fread(buffer, 1, sizeof(buffer), cache)) > 0) {
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

		// close unused pipe ends
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

	if (write(outpipefd.write, s, len) == -1) {
		perror("write(highlight)");
		exit(1);
	}

	close(outpipefd.write);

	cache = fopen(cachepath, "w+");

	n = 0;
	while ((readlen = read(inpipefd.read, buffer, sizeof buffer)) > 0) {
		fwrite(buffer, readlen, 1, fp);
		if (cache)
			fwrite(buffer, readlen, 1, cache);
		n += readlen;
	}

	close(inpipefd.read);
	if (cache)
		fclose(cache);

	waitpid(process, &status, 0);

	if (WEXITSTATUS(status))
		return -1;

	return n;
}

static size_t writeblob(const struct repoinfo* info, const char* relpath, git_object* obj, const char* fpath,
                        const char* filename, const char* staticpath, size_t filesize) {
	char        tmp[PATH_MAX] = "", *d;
	const char* p;
	size_t      lc = 0;
	FILE*       fp;

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

	writeheader(fp, info, relpath, filename, "");
	fputs("<p> ", fp);
	xmlencode(fp, filename);
	fprintf(fp, " (%zuB) <a href='%s%s'>download</a>", filesize, relpath, staticpath);
	fputs("</p><hr/>", fp);

	if (git_blob_is_binary((git_blob*) obj))
		fputs("<p>Binary file.</p>\n", fp);
	else
		lc = writeblobhtml(fp, (git_blob*) obj, filename);

	writefooter(fp);
	checkfileerror(fp, fpath, 'w');
	fclose(fp);

	relpath = "";

	return lc;
}

static void writefile(git_object* obj, const char* fpath, size_t filesize) {
	char        tmp[PATH_MAX] = "", *d;
	const char* p;
	FILE*       fp;

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


static int writefilestree(FILE* fp, const struct repoinfo* info, const char* relpath, git_tree* tree,
                          const char* path) {
	const git_tree_entry* entry = NULL;
	git_object*           obj   = NULL;
	const char*           entryname;
	char                  filepath[PATH_MAX], entrypath[PATH_MAX], staticpath[PATH_MAX], oid[8];
	size_t                count, i, lc, filesize;
	int                   r, ret;

	count = git_tree_entrycount(tree);
	for (i = 0; i < count; i++) {
		if (!(entry = git_tree_entry_byindex(tree, i)) || !(entryname = git_tree_entry_name(entry)))
			return -1;
		snprintf(entrypath, sizeof(entrypath), "%s/%s", path, entryname);

		r = snprintf(filepath, sizeof(filepath), "%s/file/%s.html", info->destdir, entrypath);
		if (r < 0 || (size_t) r >= sizeof(filepath))
			errx(1, "path truncated: '%s/file/%s.html'", info->destdir, entrypath);

		r = snprintf(staticpath, sizeof(staticpath), "%s/static/%s", info->destdir, entrypath);
		if (r < 0 || (size_t) r >= sizeof(staticpath))
			errx(1, "path truncated: '%s/static/%s'", info->destdir, entrypath);

		unhide_filename(filepath);
		unhide_filename(staticpath);

		if (!git_tree_entry_to_object(&obj, info->repo, entry)) {
			switch (git_object_type(obj)) {
				case GIT_OBJ_BLOB:
					break;
				case GIT_OBJ_TREE:
					/* NOTE: recurses */
					ret = writefilestree(fp, info, relpath, (git_tree*) obj, entrypath);
					git_object_free(obj);
					if (ret)
						return ret;
					continue;
				default:
					git_object_free(obj);
					continue;
			}

			filesize = git_blob_rawsize((git_blob*) obj);
			lc       = writeblob(info, relpath, obj, filepath, entryname, staticpath, filesize);

			writefile(obj, staticpath, filesize);

			r = snprintf(filepath, sizeof(filepath), "file/%s.html", entrypath);
			if (r < 0 || (size_t) r >= sizeof(filepath))
				errx(1, "path truncated: 'file/%s.html'", entrypath);

			r = snprintf(staticpath, sizeof(staticpath), "static/%s", entrypath);
			if (r < 0 || (size_t) r >= sizeof(staticpath))
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
			fprintf(fp, "<tr><td>m---------</td><td><a href=\"%sfile/-gitmodules.html\">", relpath);
			xmlencode(fp, entrypath);
			fputs("</a> @ ", fp);
			git_oid_tostr(oid, sizeof(oid), git_tree_entry_id(entry));
			xmlencode(fp, oid);
			fputs("</td><td class=\"num\" align=\"right\"></td></tr>\n", fp);
		}
	}

	return 0;
}

int writefiles(FILE* fp, const struct repoinfo* info, const char* relpath, const git_oid* id) {
	git_tree*   tree   = NULL;
	git_commit* commit = NULL;
	int         ret    = -1;

	fputs("<table id=\"files\"><thead>\n<tr>"
	      "<td><b>Mode</b></td><td><b>Name</b></td>"
	      "<td class=\"num\" align=\"right\"><b>Size</b></td>"
	      "</tr>\n</thead><tbody>\n",
	      fp);

	if (!git_commit_lookup(&commit, info->repo, id) && !git_commit_tree(&tree, commit))
		ret = writefilestree(fp, info, relpath, tree, "");

	fputs("</tbody></table>", fp);

	git_commit_free(commit);
	git_tree_free(tree);

	return ret;
}
