#include "gitin.h"

#include <git2/blob.h>
#include <git2/commit.h>
#include <git2/refs.h>
#include <git2/types.h>
#include <libgen.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MURMUR_SEED 0xCAFE5EED    // cafeseed

#define fallthrough __attribute__((fallthrough));


/* hash file using murmur3 */
static uint32_t filehash(const void* key, int len) {
	const uint8_t* data    = (const uint8_t*) key;
	const int      nblocks = len / 4;
	uint32_t       h1      = MURMUR_SEED;

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

static ssize_t highlight(FILE* fp, const struct repoinfo* info, struct blob* blob) {

	char* type;
	if ((type = strrchr(blob->name, '.')) != NULL)
		type++;
	else
		type = "txt";

	struct callcached_param params = {
		.command     = highlightcmd,
		.cachename   = "files",
		.content     = blob->content,
		.ncontent    = blob->length,
		.contenthash = blob->hash,
		.fp          = fp,
		.info        = info,
		.nenviron    = 3,
		.environ = (const char*[]){ "filename", blob->name, "type", type, "scheme", colorscheme },
	};

	return callcached(&params);
}

static void writeblob(const struct repoinfo* info, const char* refname, int relpath,
                      struct blob* blob) {
	char hashpath[PATH_MAX], destpath[PATH_MAX];
	int  n;

	snprintf(destpath, sizeof(destpath), "%s/.cache/blobs/%x-%s", info->destdir, blob->hash,
	         blob->name);

	if (force || access(destpath, R_OK))
		bufferwrite(blob->content, blob->length, "%s", destpath);

	n = 0;
	for (int i = 0; i < relpath; i++) {
		hashpath[n++] = '.';
		hashpath[n++] = '.';
		hashpath[n++] = '/';
	}

	snprintf(hashpath + n, sizeof(hashpath) - n, ".cache/blobs/%x-%s", blob->hash, blob->name);
	snprintf(destpath, sizeof(destpath), "%s/blob/%s/%s", info->destdir, refname, blob->path);
	unhide_path(destpath);
	unlink(destpath);
	if (symlink(hashpath, destpath))
		fprintf(stderr, "error: unable to create symlink %s -> %s\n", destpath, hashpath);
}

static void writefile(const struct repoinfo* info, const char* refname, int relpath,
                      struct blob* blob) {
	FILE* fp;
	char  hashpath[PATH_MAX], destpath[PATH_MAX];
	int   n;

	snprintf(destpath, sizeof(destpath), "%s/.cache/files/%x-%s.html", info->destdir, blob->hash,
	         blob->name);

	if (force || access(destpath, R_OK)) {
		fp = efopen(".w", "%s", destpath);
		writeheader(fp, info, relpath, info->name, "%y in %s", blob->path, refname);
		hprintf(fp, "<p> %y (%zuB) <a href='%rblob/%s/%h'>download</a></p><hr/>", blob->name,
		        blob->length, relpath, refname, blob->path);

		writepreview(fp, info, relpath, blob);

		if (blob->is_binary) {
			fputs("<p>Binary file.</p>\n", fp);
		} else if (maxfilesize != -1 && blob->length >= maxfilesize) {
			fputs("<p>File too big.</p>\n", fp);
		} else if (blob->length > 0) {
			highlight(fp, info, blob);
		}

		writefooter(fp);
		fclose(fp);
	}

	n = 0;
	for (int i = 0; i < relpath; i++) {
		hashpath[n++] = '.';
		hashpath[n++] = '.';
		hashpath[n++] = '/';
	}
	snprintf(hashpath + n, sizeof(hashpath) - n, ".cache/files/%x-%s.html", blob->hash, blob->name);
	snprintf(destpath, sizeof(destpath), "%s/file/%s/%s.html", info->destdir, refname, blob->path);
	unhide_path(destpath);
	unlink(destpath);
	if (symlink(hashpath, destpath))
		fprintf(stderr, "error: unable to create symlink %s -> %s\n", destpath, hashpath);
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

static size_t countfiles(git_repository* repo, git_tree* tree) {
	size_t entry_count = git_tree_entrycount(tree);
	size_t file_count  = 0;

	for (size_t i = 0; i < entry_count; i++) {
		const git_tree_entry* entry = git_tree_entry_byindex(tree, i);
		if (!entry)
			continue;

		git_object* object = NULL;
		if (git_tree_entry_type(entry) == GIT_OBJECT_BLOB) {
			// If it's a blob (file), increment the file count
			file_count++;
		} else if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
			// If it's a subdirectory (tree), recurse into it
			if (git_tree_entry_to_object(&object, repo, entry) == 0) {
				git_tree* subtree = (git_tree*) object;
				file_count += countfiles(repo, subtree);    // Recurse into the subtree
				git_tree_free(subtree);
			}
		}
	}
	return file_count;
}

static int writefilestree(FILE* fp, const struct repoinfo* info, const char* refname, int relpath,
                          git_tree* tree, const char* basepath, size_t* index, size_t maxfiles) {
	const git_tree_entry* entry = NULL;
	git_object*           obj   = NULL;
	const char*           entryname;
	char                  entrypath[PATH_MAX], oid[8];
	size_t                count, i;
	struct blob           blob;
	int                   dosplit;

	emkdirf(0777, "%s/file/%s/%s", info->destdir, refname, basepath);
	emkdirf(0777, "%s/blob/%s/%s", info->destdir, refname, basepath);

	dosplit = splitdirectories == -1 ? maxfiles > autofilelimit : splitdirectories;

	if (dosplit || !*basepath) {
		fp = efopen("w", "%s/file/%s/%s/index.html", info->destdir, refname, basepath);
		writeheader(fp, info, relpath, info->name, "%s in %s", basepath, refname);

		fputs("<table id=\"files\"><thead>\n<tr>"
		      "<td></td><td>Mode</td><td class=\"expand\">Name</td>"
		      "<td class=\"num\" align=\"right\">Size</td>"
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
			// addheadfile(info, entrypath);
			(void) addheadfile;

			if (git_object_type(obj) == GIT_OBJECT_BLOB) {
				hprintf(fp, "<tr><td><img src=\"%ricons/%s.svg\" /></td><td>%s</td>\n",
				        info->relpath + relpath, geticon((git_blob*) obj, entryname),
				        filemode(git_tree_entry_filemode(entry)));

				blob.name      = entryname;
				blob.path      = entrypath;
				blob.content   = git_blob_rawcontent((git_blob*) obj);
				blob.length    = git_blob_rawsize((git_blob*) obj);
				blob.is_binary = git_blob_is_binary((git_blob*) obj);
				blob.hash      = filehash(blob.content, blob.length);

				writefile(info, refname, relpath, &blob);
				writeblob(info, refname, relpath, &blob);

				if (dosplit)
					hprintf(fp, "<td><a href=\"%h.html\">%y</a></td>", entryname, entryname);
				else
					hprintf(fp, "<td><a href=\"%h.html\">%y</a></td>", entrypath, entrypath);
				fputs("<td class=\"num\" align=\"right\">", fp);
				fprintf(fp, "%zuB", blob.length);
				fputs("</td></tr>\n", fp);

				(*index)++;

				printprogress(*index, maxfiles, "write files: %-20s", refname);
			} else if (git_object_type(obj) == GIT_OBJECT_TREE) {
				if (dosplit) {
					hprintf(
					    fp,
					    "<tr><td><img src=\"%ricons/directory.svg\" /></td><td>d---------</td><td colspan=\"2\"><a href=\"%h/\">%y</a></td></tr>\n",
					    info->relpath + relpath, entrypath, entrypath);
				}
				writefilestree(fp, info, refname, relpath + 1, (git_tree*) obj, entrypath, index,
				               maxfiles);
			}

			git_object_free(obj);
		} else if (git_tree_entry_type(entry) == GIT_OBJECT_COMMIT) {
			git_oid_tostr(oid, sizeof(oid), git_tree_entry_id(entry));
			hprintf(
			    fp,
			    "<tr><td></td><td>m---------</td><td><a href=\"file/%s/-gitmodules.html\">%y</a>",
			    refname, entrypath);
			hprintf(fp, " @ %y</td><td class=\"num\" align=\"right\"></td></tr>\n", oid);
		}
	}

	if (dosplit || !*basepath) {
		fputs("</tbody></table>", fp);
		writefooter(fp);
		fclose(fp);
	}

	return 0;
}

int writefiles(const struct repoinfo* info, const char* refname, git_commit* commit) {
	git_tree* tree = NULL;
	size_t    indx = 0;
	int       ret  = -1;
	char      path[PATH_MAX];
	char      headoid[GIT_OID_SHA1_HEXSIZE + 1], oid[GIT_OID_SHA1_HEXSIZE + 1];

	git_oid_tostr(headoid, sizeof(headoid), git_commit_id(commit));
	if (!force) {
		if (!bufferread(oid, GIT_OID_SHA1_HEXSIZE, "%s/.cache/filetree", info->destdir)) {
			oid[GIT_OID_SHA1_HEXSIZE] = '\0';
			if (!strcmp(oid, headoid))
				return 0;
		}
	}

	// Clean /file and /blob directories since they will be rewritten
	snprintf(path, sizeof(path), "%s/file/%s", info->destdir, refname);
	removedir(path);
	snprintf(path, sizeof(path), "%s/blob/%s", info->destdir, refname);
	removedir(path);

	emkdirf(0777, "%s/file", info->destdir);
	emkdirf(0777, "%s/file/%s", info->destdir, refname);
	emkdirf(0777, "%s/blob", info->destdir);
	emkdirf(0777, "%s/blob/%s", info->destdir, refname);

	if (!git_commit_tree(&tree, commit)) {
		ret = writefilestree(NULL, info, refname, 2, tree, "", &indx, countfiles(info->repo, tree));

		if (!verbose)
			fputc('\n', stdout);
	}

	git_tree_free(tree);

	bufferwrite(headoid, GIT_OID_SHA1_HEXSIZE, "%s/.cache/filetree", info->destdir);

	return ret;
}
