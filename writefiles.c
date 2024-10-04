#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "writer.h"

#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MURMUR_SEED 0xCAFE5EED    // cafeseed

#define fallthrough __attribute__((fallthrough));

/* a simple and fast hashing function */
static uint32_t murmurhash3(const void* key, int len, uint32_t seed) {
	const uint8_t* data    = (const uint8_t*) key;
	const int      nblocks = len / 4;
	uint32_t       h1      = seed;

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	// Body
	const uint32_t* blocks = (const uint32_t*) (data + nblocks * 4);
	for (int i = -nblocks; i; i++) {
		uint32_t k1 = blocks[i];

		k1 *= c1;
		k1 = (k1 << 15) | (k1 >> (32 - 15));
		k1 *= c2;

		h1 ^= k1;
		h1 = (h1 << 13) | (h1 >> (32 - 13));
		h1 = h1 * 5 + 0xe6546b64;
	}

	// Tail
	const uint8_t* tail = (const uint8_t*) (data + nblocks * 4);
	uint32_t       k1   = 0;

	switch (len & 3) {
		case 3:
			k1 ^= tail[2] << 16;
			fallthrough;
		case 2:
			k1 ^= tail[1] << 8;
			fallthrough;
		case 1:
			k1 ^= tail[0];
			k1 *= c1;
			k1 = (k1 << 15) | (k1 >> (32 - 15));
			k1 *= c2;
			h1 ^= k1;
	}

	// Finalization
	h1 ^= len;
	h1 ^= h1 >> 16;
	h1 *= 0x85ebca6b;
	h1 ^= h1 >> 13;
	h1 *= 0xc2b2ae35;
	h1 ^= h1 >> 16;

	return h1;
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

static ssize_t highlight(FILE* fp, const struct repoinfo* info, const git_blob* blob,
                         const char* filename) {
	static unsigned char buffer[512];
	static char          cachepath[PATH_MAX];
	ssize_t              n = 0, len;
	const char*          s = git_blob_rawcontent(blob);
	FILE*                cache;
	pipe_t               inpipefd;
	pipe_t               outpipefd;
	pid_t                process;
	ssize_t              readlen;
	int                  status;
	uint32_t             contenthash;
	char*                type;

	len = git_blob_rawsize(blob);
	if (len == 0)
		return 0;

	if (maxfilesize != -1 && len >= maxfilesize) {
		fputs("<p>File too big.</p>\n", fp);
		return 0;
	}

	fflush(stdout);

	contenthash = murmurhash3(s, len, MURMUR_SEED);
	snprintf(cachepath, sizeof(cachepath), "%s/.gitin/files/%x", info->destdir, contenthash);
	normalize_path(cachepath);

	if (!force && (cache = fopen(cachepath, "r"))) {
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
		hprintf(stderr, "error: unable to fork process: %w\n");
		exit(100);    // Fatal error, exit with 100
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

		hprintf(stderr, "error: unable to exec highlight: %w\n");
		_exit(100);    // Fatal error inside child
	}

	close(outpipefd.read);
	close(inpipefd.write);

	if (write(outpipefd.write, s, len) == -1) {
		hprintf(stderr, "error: unable to write to pipe: %w\n");
		exit(100);    // Fatal error, exit with 100
		goto wait;
	}

	close(outpipefd.write);

	if ((cache = fopen(cachepath, "w+"))) {
		if (verbose)
			fprintf(stderr, "%s\n",
			        cachepath);    // Keeping standard fprintf since it's just logging the path
	}

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

static size_t writeblob(const struct repoinfo* info, int relpath, git_object* obj,
                        const char* fpath, const char* filename, const char* filepath,
                        size_t filesize) {

	char   tmp[PATH_MAX] = "", *d;
	size_t lc            = 0;
	FILE*  fp;

	strlcpy(tmp, fpath, sizeof(tmp));
	d = dirname(tmp);

	if (mkdirp(d, 0777)) {
		hprintf(stderr, "error: unable to create directory: %w\n");
		return -1;
	}

	fp = xfopen("w", "%s", fpath);
	writeheader(fp, info, relpath, info->name, "%y", filepath);
	hprintf(fp, "<p> %y (%zuB) <a href='%rblob%h'>download</a></p><hr/>", filename, filesize,
	        relpath, filepath);

	if (git_blob_is_binary((git_blob*) obj)) {
		fputs("<p>Binary file.</p>\n", fp);
	} else {
		lc = highlight(fp, info, (git_blob*) obj, filename);
	}

	writefooter(fp);
	fclose(fp);

	return lc;
}

static void writefile(git_object* obj, const char* fpath, size_t filesize) {
	char  tmp[PATH_MAX] = "", *d;
	FILE* fp;

	strlcpy(tmp, fpath, sizeof(tmp));
	d = dirname(tmp);

	if (mkdirp(d, 0777)) {
		hprintf(stderr, "error: unable to create directory: %w\n");
		return;
	}

	fp = xfopen("w", "%s", fpath);
	fwrite(git_blob_rawcontent((const git_blob*) obj), filesize, 1, fp);
	fclose(fp);
}

static void addheadfile(struct repoinfo* info, const char* filename) {
	// Reallocate more space if needed
	if (info->headfileslen >= info->headfilesalloc) {
		int    new_alloc = info->headfilesalloc == 0 ? 4 : info->headfilesalloc * 2;
		char** temp      = realloc(info->headfiles, new_alloc * sizeof(char*));
		if (temp == NULL) {
			hprintf(stderr, "error: unable to realloc memory: %w\n");
			exit(100);
		}
		info->headfiles      = temp;
		info->headfilesalloc = new_alloc;
	}

	// Copy the string and store it in the list
	info->headfiles[info->headfileslen++] = strdup(filename);
}

static int endswith(const char* str, const char* suffix) {
	size_t lenstr    = strlen(str);
	size_t lensuffix = strlen(suffix);
	if (lensuffix > lenstr)
		return 0;
	return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

// List of known file extensions and their types
static const char* file_types[][2] = {
	{ ".png", "image" },  { ".jpg", "image" },    { ".jpeg", "image" },      { ".gif", "image" },
	{ ".bin", "blob" },   { ".exe", "blob" },     { ".o", "blob" },          { ".c", "source" },
	{ ".cpp", "source" }, { ".h", "source" },     { ".py", "source" },       { ".txt", "text" },
	{ ".md", "text" },    { "README", "readme" }, { "README.md", "readme" }, { NULL, NULL },
};

static const char* geticon(const char* filename) {
	// Iterate over the list of file types
	for (int i = 0; file_types[i][0] != NULL; i++) {
		if (endswith(filename, file_types[i][0])) {
			return file_types[i][1];
		}
	}
	return "other";
}

static int writefilestree(FILE* fp, struct repoinfo* info, int relpath, git_tree* tree,
                          const char* path) {
	const git_tree_entry* entry = NULL;
	git_object*           obj   = NULL;
	const char*           entryname;
	char                  filepath[PATH_MAX], entrypath[PATH_MAX], staticpath[PATH_MAX], oid[8];
	size_t                count, i, lc, filesize;

	if (filesperdirectory) {
		snprintf(entrypath, sizeof(entrypath), "%s/file/%s", info->destdir, path);
		mkdir(entrypath, 0777);

		fp = xfopen("w+", "%s/file/%s/index.html", info->destdir, path);
		writeheader(fp, info, relpath, info->name, "%s", path);

		fputs("<table id=\"files\"><thead>\n<tr>"
		      "<td></td><td><b>Mode</b></td><td class=\"expand\"><b>Name</b></td>"
		      "<td class=\"num\" align=\"right\"><b>Size</b></td>"
		      "</tr>\n</thead><tbody>\n",
		      fp);
	}

	count = git_tree_entrycount(tree);
	for (i = 0; i < count; i++) {
		if (!(entry = git_tree_entry_byindex(tree, i)) || !(entryname = git_tree_entry_name(entry)))
			return -1;
		snprintf(entrypath, sizeof(entrypath), "%s/%s", path, entryname);

		if (!git_tree_entry_to_object(&obj, info->repo, entry)) {
			addheadfile(info, entrypath + 1);    // +1 to remove leading slash

			// this weird useless (void) (... == 0) is because gcc will complain about truncation
			(void) (snprintf(filepath, sizeof(filepath), "%s/file/%s.html", info->destdir,
			                 entrypath) == 0);
			(void) (snprintf(staticpath, sizeof(staticpath), "%s/blob/%s", info->destdir,
			                 entrypath) == 0);

			normalize_path(filepath);
			normalize_path(staticpath);
			unhide_path(filepath);
			unhide_path(staticpath);

			if (git_object_type(obj) == GIT_OBJ_BLOB) {
				hprintf(fp, "<tr><td><img src=\"%ricons/%s.svg\" /></td><td>%s</td>\n",
				        info->relpath + relpath, geticon(entryname),
				        filemode(git_tree_entry_filemode(entry)));
				filesize = git_blob_rawsize((git_blob*) obj);
				lc       = writeblob(info, relpath, obj, filepath, entryname, entrypath, filesize);

				writefile(obj, staticpath, filesize);

				hprintf(fp, "<td><a href=\"%rfile%h.html\">%y</a></td>", relpath, entrypath,
				        entrypath + 1);
				fputs("<td class=\"num\" align=\"right\">", fp);
				if (lc > 0)
					fprintf(fp, "%zuL", lc);
				else
					fprintf(fp, "%zuB", filesize);
			} else {
				if (filesperdirectory) {
					hprintf(
					    fp,
					    "<tr><td><img src=\"%ricons/directory.svg\" /></td><td>d---------</td><td colspan=2><a href=\"%h/\">%y</a></td>\n",
					    info->relpath + relpath, entrypath + 1, entrypath + 1);
					writefilestree(NULL, info, relpath + 1, (git_tree*) obj, entrypath);
				} else {
					writefilestree(fp, info, relpath + 1, (git_tree*) obj, entrypath);
				}
				// error?
			}
			fputs("</td></tr>\n", fp);

			git_object_free(obj);
		} else if (git_tree_entry_type(entry) == GIT_OBJ_COMMIT) {
			/* commit object in tree is a submodule */
			git_oid_tostr(oid, sizeof(oid), git_tree_entry_id(entry));
			hprintf(fp,
			        "<tr><td></td><td>m---------</td><td><a href=\"file/-gitmodules.html\">%y</a>",
			        entrypath);
			hprintf(fp, " @ %y</td><td class=\"num\" align=\"right\"></td></tr>\n", oid);
		}
	}

	if (filesperdirectory) {
		fputs("</tbody></table>", fp);
		writefooter(fp);
		fclose(fp);
	}

	return 0;
}

int writefiles(struct repoinfo* info) {
	git_tree*   tree   = NULL;
	git_commit* commit = NULL;
	int         ret    = -1;
	char        path[PATH_MAX];
	FILE *      cache, *fp = NULL;
	char        headoid[GIT_OID_HEXSZ + 1], oid[GIT_OID_HEXSZ + 1];

	if (!force && info->head) {
		git_oid_tostr(headoid, sizeof(headoid), info->head);

		snprintf(path, sizeof(path), "%s/.gitin/filetree", info->destdir);
		if ((cache = fopen(path, "r"))) {
			fread(oid, GIT_OID_HEXSZ, 1, cache);
			oid[GIT_OID_HEXSZ] = '\0';
			fclose(cache);

			if (!strcmp(oid, headoid)) {
				return 0;
			}
		}
	}

	// clean /file and /blob because they're rewritten nontheless
	snprintf(path, sizeof(path), "%s/file", info->destdir);
	removedir(path);
	snprintf(path, sizeof(path), "%s/blob", info->destdir);
	removedir(path);

	if (!filesperdirectory) {
		/* files for HEAD, it must be before writelog, as it also populates headfiles! */
		fp = xfopen("w", "%s/%s", info->destdir, treefile);
		writeheader(fp, info, 0, info->name, "%y", info->description);

		fputs("<table id=\"files\"><thead>\n<tr>"
		      "<td></td><td><b>Mode</b></td><td class=\"expand\"><b>Name</b></td>"
		      "<td class=\"num\" align=\"right\"><b>Size</b></td>"
		      "</tr>\n</thead><tbody>\n",
		      fp);
	}

	if (info->head && !git_commit_lookup(&commit, info->repo, info->head) &&
	    !git_commit_tree(&tree, commit))
		ret = writefilestree(fp, info, 1, tree, "");

	git_tree_free(tree);

	if (!filesperdirectory) {
		fputs("</tbody></table>", fp);
		writefooter(fp);
		fclose(fp);
	}

	snprintf(path, sizeof(path), "%s/.gitin/filetree", info->destdir);
	if ((cache = fopen(path, "w"))) {
		if (verbose)
			fprintf(stderr, "%s\n", path);
		fwrite(headoid, GIT_OID_HEXSZ, 1, cache);
		fclose(cache);
	}

	git_commit_free(commit);

	return ret;
}
