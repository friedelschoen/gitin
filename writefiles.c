#include "gitin.h"

#include <git2/blob.h>
#include <git2/commit.h>
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

static ssize_t highlight(FILE* fp, const struct repoinfo* info, const char* filename, const char* s,
                         ssize_t len, uint32_t contenthash) {

	char* type;
	if ((type = strrchr(filename, '.')) != NULL)
		type++;
	else
		type = "txt";

	struct callcached_param params = {
		.command     = highlightcmd,
		.cachename   = "files",
		.content     = s,
		.ncontent    = len,
		.contenthash = contenthash,
		.fp          = fp,
		.info        = info,
		.nenviron    = 3,
		.environ     = (const char*[]){ "filename", filename, "type", type, "scheme", colorscheme },
	};

	return callcached(&params);
}

static size_t writeblob(const struct repoinfo* info, int relpath, git_blob* obj,
                        const char* filename, const char* filepath, ssize_t filesize) {
	size_t      lc = 0;
	FILE*       fp;
	const void* content = git_blob_rawcontent(obj);
	uint32_t    contenthash;

	bufferwrite(content, filesize, "%s/blob/%s", info->destdir, filepath);

	fp = xfopen("w", "%s/file/%s.html", info->destdir, filepath);
	writeheader(fp, info, relpath, info->name, "%y", filepath);
	hprintf(fp, "<p> %y (%zuB) <a href='%rblob/%h'>download</a></p><hr/>", filename, filesize,
	        relpath, filepath);

	contenthash = filehash(content, filesize);

	writepreview(fp, info, relpath, filepath, content, filesize, contenthash);

	if (git_blob_is_binary(obj)) {
		fputs("<p>Binary file.</p>\n", fp);
	} else if (maxfilesize != -1 && filesize >= maxfilesize) {
		fputs("<p>File too big.</p>\n", fp);
	} else if (filesize > 0) {
		lc = highlight(fp, info, filepath, content, filesize, contenthash);
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

static int writefilestree(FILE* fp, struct repoinfo* info, int relpath, git_tree* tree,
                          const char* basepath, size_t* index, size_t maxfiles) {
	const git_tree_entry* entry = NULL;
	git_object*           obj   = NULL;
	const char*           entryname;
	char                  entrypath[PATH_MAX], oid[8];
	size_t                count, i, lc, filesize;

	xmkdirf(0777, "%s/file/%s", info->destdir, basepath);
	xmkdirf(0777, "%s/blob/%s", info->destdir, basepath);

	if (splitdirectories || !*basepath) {
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

			if (git_object_type(obj) == GIT_OBJECT_BLOB) {
				hprintf(fp, "<tr><td><img src=\"%ricons/%s.svg\" /></td><td>%s</td>\n",
				        info->relpath + relpath, geticon((git_blob*) obj, entryname),
				        filemode(git_tree_entry_filemode(entry)));
				filesize = git_blob_rawsize((git_blob*) obj);
				lc = writeblob(info, relpath, (git_blob*) obj, entryname, entrypath, filesize);

				if (splitdirectories)
					hprintf(fp, "<td><a href=\"%h.html\">%y</a></td>", entryname, entryname);
				else
					hprintf(fp, "<td><a href=\"%h.html\">%y</a></td>", entrypath, entrypath);
				fputs("<td class=\"num\" align=\"right\">", fp);
				if (lc > 0)
					fprintf(fp, "%zuL", lc);
				else
					fprintf(fp, "%zuB", filesize);
				fputs("</td></tr>\n", fp);

				(*index)++;

				printprogress("write files:", *index, maxfiles);
			} else if (git_object_type(obj) == GIT_OBJECT_TREE) {
				if (splitdirectories) {
					hprintf(
					    fp,
					    "<tr><td><img src=\"%ricons/directory.svg\" /></td><td>d---------</td><td colspan=\"2\"><a href=\"%h/\">%y</a></td></tr>\n",
					    info->relpath + relpath, entrypath, entrypath);
				}
				writefilestree(fp, info, relpath + 1, (git_tree*) obj, entrypath, index, maxfiles);
			}

			git_object_free(obj);
		} else if (git_tree_entry_type(entry) == GIT_OBJECT_COMMIT) {
			git_oid_tostr(oid, sizeof(oid), git_tree_entry_id(entry));
			hprintf(fp,
			        "<tr><td></td><td>m---------</td><td><a href=\"file/-gitmodules.html\">%y</a>",
			        entrypath);
			hprintf(fp, " @ %y</td><td class=\"num\" align=\"right\"></td></tr>\n", oid);
		}
	}

	if (splitdirectories || !*basepath) {
		fputs("</tbody></table>", fp);
		writefooter(fp);
		fclose(fp);
	}

	return 0;
}

int writefiles(struct repoinfo* info) {
	git_tree* tree = NULL;
	size_t    indx = 0;
	int       ret  = -1;
	char      path[PATH_MAX];
	char      headoid[GIT_OID_SHA1_HEXSIZE + 1], oid[GIT_OID_SHA1_HEXSIZE + 1];

	if (!force) {
		git_oid_tostr(headoid, sizeof(headoid), git_commit_id(info->commit));

		if (!bufferread(oid, GIT_OID_SHA1_HEXSIZE, "%s/.cache/filetree", info->destdir)) {
			oid[GIT_OID_SHA1_HEXSIZE] = '\0';
			if (!strcmp(oid, headoid))
				return 0;
		}
	}

	// Clean /file and /blob directories since they will be rewritten
	snprintf(path, sizeof(path), "%s/file", info->destdir);
	removedir(path);
	snprintf(path, sizeof(path), "%s/blob", info->destdir);
	removedir(path);

	xmkdirf(0777, "%s/file", info->destdir);
	xmkdirf(0777, "%s/blob", info->destdir);

	if (!git_commit_tree(&tree, info->commit)) {
		ret = writefilestree(NULL, info, 1, tree, "", &indx, countfiles(info->repo, tree));

		if (!verbose)
			fputc('\n', stdout);
	}

	git_tree_free(tree);

	bufferwrite(headoid, GIT_OID_SHA1_HEXSIZE, "%s/.cache/filetree", info->destdir);

	return ret;
}
