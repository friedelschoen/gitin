#include "common.h"
#include "compat.h"
#include "config.h"
#include "hprintf.h"
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

static ssize_t writeblobhtml(FILE* fp, const struct repoinfo* info, const git_blob* blob, const char* filename) {
	ssize_t     n = 0, len;
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

	if (maxfilesize != -1 && len >= maxfilesize) {
		fputs("<p>File too big.</p>\n", fp);
		return 0;
	}

	fflush(stdout);

	contenthash = murmurhash3(s, len, MURMUR_SEED);
	snprintf(cachepath, sizeof(cachepath), "%s/%s/%x", info->destdir, highlightcache, contenthash);
	normalize_path(cachepath);

	if ((cache = fopen(cachepath, "r"))) {
		n = 0;
		while ((readlen = fread(buffer, 1, sizeof(buffer), cache)) > 0) {
			fwrite(buffer, 1, readlen, fp);
			n += readlen;
		}
		fclose(cache);

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
		setenv("scheme", colorscheme, 1);
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
		goto wait;
	}

	close(outpipefd.write);

	cache = fopen(cachepath, "w+");
	fprintf(stderr, "%s\n", cachepath);

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

wait:
	waitpid(process, &status, 0);

	return WEXITSTATUS(status) ? -1 : n;
}

static size_t writeblob(const struct repoinfo* info, int relpath, git_object* obj, const char* fpath,
                        const char* filename, const char* filepath, size_t filesize) {

	char   tmp[PATH_MAX] = "", *d;
	size_t lc            = 0;
	FILE*  fp;

	if (strlcpy(tmp, fpath, sizeof(tmp)) >= sizeof(tmp))
		errx(1, "path truncated: '%s'", fpath);
	if (!(d = dirname(tmp)))
		err(1, "dirname");

	if (mkdirp(d))
		return -1;

	if (!(fp = fopen(fpath, "w")))
		err(1, "fopen: '%s'", fpath);
	fprintf(stderr, "%s\n", fpath);

	writeheader(fp, info, relpath, info->name, "%y", filepath);
	hprintf(fp, "<p> %y (%zuB) <a href='%rblob%h'>download</a></p><hr/>", filename, filesize, relpath, filepath);

	if (git_blob_is_binary((git_blob*) obj))
		fputs("<p>Binary file.</p>\n", fp);
	else
		lc = writeblobhtml(fp, info, (git_blob*) obj, filename);

	writefooter(fp);
	checkfileerror(fp, fpath, 'w');
	fclose(fp);

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
	fprintf(stderr, "%s\n", fpath);

	fwrite(git_blob_rawcontent((const git_blob*) obj), filesize, 1, fp);
	checkfileerror(fp, fpath, 'w');
	fclose(fp);
}

static void addheadfile(struct repoinfo* info, const char* filename) {
	// Reallocate more space if needed
	if (info->headfileslen >= info->headfilesalloc) {
		int    new_alloc = info->headfilesalloc == 0 ? 4 : info->headfilesalloc * 2;
		char** temp      = realloc(info->headfiles, new_alloc * sizeof(char*));
		if (temp == NULL) {
			perror("Failed to realloc");
			return;
		}
		info->headfiles      = temp;
		info->headfilesalloc = new_alloc;
	}

	// Copy the string and store it in the list
	if ((info->headfiles[info->headfileslen++] = strdup(filename)) == NULL) {
		perror("Failed to strdup");
		exit(1);
	}
}

static int writefilestree(FILE* fp, struct repoinfo* info, int relpath, git_tree* tree, const char* path) {
	const git_tree_entry* entry = NULL;
	git_object*           obj   = NULL;
	const char*           entryname;
	char                  filepath[PATH_MAX], entrypath[PATH_MAX], staticpath[PATH_MAX], oid[8];
	size_t                count, i, lc, filesize;
	int                   ret;

	count = git_tree_entrycount(tree);
	for (i = 0; i < count; i++) {
		if (!(entry = git_tree_entry_byindex(tree, i)) || !(entryname = git_tree_entry_name(entry)))
			return -1;
		snprintf(entrypath, sizeof(entrypath), "%s/%s", path, entryname);

		if (!git_tree_entry_to_object(&obj, info->repo, entry)) {
			switch (git_object_type(obj)) {
				case GIT_OBJ_BLOB:
					break;
				case GIT_OBJ_TREE:
					/* NOTE: recurses */
					ret = writefilestree(fp, info, relpath + 1, (git_tree*) obj, entrypath);
					git_object_free(obj);
					if (ret)
						return ret;
					continue;
				default:
					git_object_free(obj);
					continue;
			}

			addheadfile(info, entrypath + 1);    // +1 to remove leading slash

			snprintf(filepath, sizeof(filepath), "%s/file/%s.html", info->destdir, entrypath);
			snprintf(staticpath, sizeof(staticpath), "%s/blob/%s", info->destdir, entrypath);

			normalize_path(filepath);
			normalize_path(staticpath);
			unhide_path(filepath);
			unhide_path(staticpath);

			filesize = git_blob_rawsize((git_blob*) obj);
			lc       = writeblob(info, relpath, obj, filepath, entryname, entrypath, filesize);

			writefile(obj, staticpath, filesize);

			fprintf(fp, "<tr><td>%s</td>\n", filemode(git_tree_entry_filemode(entry)));
			hprintf(fp, "<td><a href=\"file%h.html\">%y</a></td>", entrypath, entrypath + 1);
			fputs("<td class=\"num\" align=\"right\">", fp);
			if (lc > 0)
				fprintf(fp, "%zuL", lc);
			else
				fprintf(fp, "%zuB", filesize);
			fputs("</td></tr>\n", fp);
			git_object_free(obj);
		} else if (git_tree_entry_type(entry) == GIT_OBJ_COMMIT) {
			/* commit object in tree is a submodule */
			git_oid_tostr(oid, sizeof(oid), git_tree_entry_id(entry));
			hprintf(fp, "<tr><td>m---------</td><td><a href=\"file/-gitmodules.html\">%y</a>", entrypath);
			hprintf(fp, " @ %y</td><td class=\"num\" align=\"right\"></td></tr>\n", oid);
		}
	}

	return 0;
}

int writefiles(FILE* fp, struct repoinfo* info) {
	git_tree*   tree   = NULL;
	git_commit* commit = NULL;
	int         ret    = -1;
	char        path[PATH_MAX];

	// clean /file and /blob because they're rewritten nontheless
	snprintf(path, sizeof(path), "%s/file", info->destdir);
	removedir(path);
	snprintf(path, sizeof(path), "%s/blob", info->destdir);
	removedir(path);

	fputs("<table id=\"files\"><thead>\n<tr>"
	      "<td><b>Mode</b></td><td class=\"expand\"><b>Name</b></td>"
	      "<td class=\"num\" align=\"right\"><b>Size</b></td>"
	      "</tr>\n</thead><tbody>\n",
	      fp);

	if (!git_commit_lookup(&commit, info->repo, info->head) && !git_commit_tree(&tree, commit))
		ret = writefilestree(fp, info, 1, tree, "");

	fputs("</tbody></table>", fp);

	git_commit_free(commit);
	git_tree_free(tree);

	return ret;
}
