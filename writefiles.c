#include "common.h"
#include "config.h"
#include "filetypes.h"
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

static ssize_t highlight(FILE* fp, const struct repoinfo* info, const char* filename, const char* s,
                         ssize_t len) {
	static unsigned char buffer[512];
	ssize_t              n = 0;
	FILE*                cache;
	pipe_t               inpipefd;
	pipe_t               outpipefd;
	pid_t                process;
	ssize_t              readlen;
	int                  status;
	uint32_t             contenthash;
	char*                type;

	if (maxfilesize != -1 && len >= maxfilesize) {
		fputs("<p>File too big.</p>\n", fp);
		return 0;
	}

	fflush(stdout);

	contenthash = murmurhash3(s, len, MURMUR_SEED);

	if (!force && (cache = xfopen(".!r", "%s/.gitin/files/%x", info->destdir, contenthash))) {
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
	else
		type = "txt";

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

	cache = xfopen(".!w+", "%s/.gitin/files/%x", info->destdir, contenthash);

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

static size_t writeblob(const struct repoinfo* info, int relpath, git_blob* obj,
                        const char* filename, const char* filepath, size_t filesize) {
	size_t      lc = 0;
	FILE*       fp;
	const void* content = git_blob_rawcontent(obj);

	fp = xfopen("w", "%s/blob/%s", info->destdir, filepath);
	fwrite(content, filesize, 1, fp);
	fclose(fp);

	fp = xfopen("w", "%s/file/%s.html", info->destdir, filepath);
	writeheader(fp, info, relpath, info->name, "%y", filepath);
	hprintf(fp, "<p> %y (%zuB) <a href='%rblob/%h'>download</a></p><hr/>", filename, filesize,
	        relpath, filepath);

	if (git_blob_is_binary(obj)) {
		fputs("<p>Binary file.</p>\n", fp);
	} else if (filesize > 0) {
		lc = highlight(fp, info, filename, content, filesize);
	}

	writefooter(fp);
	fclose(fp);

	return lc;
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

static const char* geticon(const git_blob* blob, const char* filename) {
	if (strstr(filename, "README"))
		return "readme";

	for (int i = 0; filetypes[i][0] != NULL; i++) {
		if (endswith(filename, filetypes[i][0])) {
			return filetypes[i][1];
		}
	}

	return git_blob_is_binary(blob) ? "binary" : "other";
}

static int writefilestree(FILE* fp, struct repoinfo* info, int relpath, git_tree* tree,
                          const char* basepath) {
	const git_tree_entry* entry = NULL;
	git_object*           obj   = NULL;
	const char*           entryname;
	char                  entrypath[PATH_MAX], oid[8];
	size_t                count, i, lc, filesize;

	xmkdirf(0777, "%s/file/%s", info->destdir, basepath);
	xmkdirf(0777, "%s/blob/%s", info->destdir, basepath);

	if (filesperdirectory || !*basepath) {
		fp = xfopen("w", "%s/file/%s/index.html", info->destdir, basepath);
		writeheader(fp, info, relpath, info->name, "%s", basepath);

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
		if (*basepath)
			snprintf(entrypath, sizeof(entrypath), "%s/%s", basepath, entryname);
		else
			strlcpy(entrypath, entryname, sizeof(entrypath));

		if (!git_tree_entry_to_object(&obj, info->repo, entry)) {
			addheadfile(info, entrypath);

			if (git_object_type(obj) == GIT_OBJ_BLOB) {
				hprintf(fp, "<tr><td><img src=\"%ricons/%s.svg\" /></td><td>%s</td>\n",
				        info->relpath + relpath, geticon((git_blob*) obj, entryname),
				        filemode(git_tree_entry_filemode(entry)));
				filesize = git_blob_rawsize((git_blob*) obj);
				lc = writeblob(info, relpath, (git_blob*) obj, entryname, entrypath, filesize);

				if (filesperdirectory)
					hprintf(fp, "<td><a href=\"%h.html\">%y</a></td>", entryname, entryname);
				else
					hprintf(fp, "<td><a href=\"%h.html\">%y</a></td>", entrypath, entrypath);
				fputs("<td class=\"num\" align=\"right\">", fp);
				if (lc > 0)
					fprintf(fp, "%zuL", lc);
				else
					fprintf(fp, "%zuB", filesize);
				fputs("</td></tr>\n", fp);
			} else if (git_object_type(obj) == GIT_OBJ_TREE) {
				if (filesperdirectory) {
					hprintf(
					    fp,
					    "<tr><td><img src=\"%ricons/directory.svg\" /></td><td>d---------</td><td colspan=\"2\"><a href=\"%h/\">%y</a></td><tr>\n",
					    info->relpath + relpath, entrypath, entrypath);
				}
				writefilestree(fp, info, relpath + !!filesperdirectory, (git_tree*) obj, entrypath);
			}

			git_object_free(obj);
		} else if (git_tree_entry_type(entry) == GIT_OBJ_COMMIT) {
			git_oid_tostr(oid, sizeof(oid), git_tree_entry_id(entry));
			hprintf(fp,
			        "<tr><td></td><td>m---------</td><td><a href=\"file/-gitmodules.html\">%y</a>",
			        entrypath);
			hprintf(fp, " @ %y</td><td class=\"num\" align=\"right\"></td></tr>\n", oid);
		}
	}

	if (filesperdirectory || !*basepath) {
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
	FILE*       cache;
	char        headoid[GIT_OID_HEXSZ + 1], oid[GIT_OID_HEXSZ + 1];

	if (!force && info->head) {
		git_oid_tostr(headoid, sizeof(headoid), info->head);

		if ((cache = xfopen(".!r", "%s/.gitin/filetree", info->destdir))) {
			fread(oid, GIT_OID_HEXSZ, 1, cache);
			oid[GIT_OID_HEXSZ] = '\0';
			fclose(cache);

			if (!strcmp(oid, headoid)) {
				return 0;
			}
		}
	}

	// Clean /file and /blob directories since they will be rewritten
	snprintf(path, sizeof(path), "%s/file", info->destdir);
	removedir(path);
	snprintf(path, sizeof(path), "%s/blob", info->destdir);
	removedir(path);

	xmkdirf(0777, "%s/file", info->destdir);
	xmkdirf(0777, "%s/blob", info->destdir);

	if (info->head && !git_commit_lookup(&commit, info->repo, info->head) &&
	    !git_commit_tree(&tree, commit)) {
		ret = writefilestree(NULL, info, 1, tree, "");
	}

	git_tree_free(tree);

	if ((cache = xfopen(".!w", "%s/.gitin/filetree", info->destdir))) {
		fwrite(headoid, GIT_OID_HEXSZ, 1, cache);
		fclose(cache);
	}

	git_commit_free(commit);

	return ret;
}
