#include "buffer.h"
#include "common.h"
#include "config.h"
#include "getinfo.h"
#include "hprintf.h"
#include "writer.h"

#include <archive.h>
#include <archive_entry.h>
#include <git2/blob.h>
#include <git2/commit.h>
#include <limits.h>
#include <string.h>

/* Function to write a blob (file) from the repository to the archive */
static int writearchiveblob(git_blob* blob, const char* path, const git_tree_entry* gitentry,
                            struct archive* a) {
	struct archive_entry* entry;
	entry = archive_entry_new();
	archive_entry_set_pathname(entry, path);
	archive_entry_set_size(entry, git_blob_rawsize(blob));
	archive_entry_set_mode(entry, git_tree_entry_filemode(gitentry));

	if (archive_write_header(a, entry) != ARCHIVE_OK) {
		fprintf(stderr, "error: unable to write header for %s: %s\n", path,
		        archive_error_string(a));
		archive_entry_free(entry);
		return -1;
	}

	const void* blob_content = git_blob_rawcontent(blob);
	if (blob_content && git_blob_rawsize(blob) > 0) {
		if (archive_write_data(a, blob_content, git_blob_rawsize(blob)) < 0) {
			fprintf(stderr, "error: unable to write data for %s: %s\n", path,
			        archive_error_string(a));
			archive_entry_free(entry);
			return -1;
		}
	}

	archive_entry_free(entry);
	return 0;
}

/* Recursively process the tree to archive files */
static int walktree(git_repository* repo, git_tree* tree, const char* base_path,
                    struct archive* a) {
	size_t count = git_tree_entrycount(tree);
	for (size_t i = 0; i < count; ++i) {
		const git_tree_entry* entry = git_tree_entry_byindex(tree, i);
		const char*           name  = git_tree_entry_name(entry);
		char                  full_path[1024];
		snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);

		if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
			git_tree* subtree;
			if (git_tree_entry_to_object((git_object**) &subtree, repo, entry) != 0) {
				hprintf(stderr, "error: unable to load git-tree: %gw\n");
				return -1;
			}
			walktree(repo, subtree, full_path, a);
			git_tree_free(subtree);
		} else if (git_tree_entry_type(entry) == GIT_OBJECT_BLOB) {
			git_blob* blob;
			if (git_tree_entry_to_object((git_object**) &blob, repo, entry) != 0) {
				hprintf(stderr, "error: unable to load blob: %gw\n");
				return -1;
			}
			if (writearchiveblob(blob, full_path, entry, a) != 0) {
				git_blob_free(blob);
				return -1;
			}
			git_blob_free(blob);
		}
	}
	return 0;
}

/* Updated function to accept git_reference instead of branch/tag name */
int writearchive(FILE* fp, const struct repoinfo* info, int type, struct referenceinfo* refinfo) {
	git_tree*       tree = NULL;
	char            path[PATH_MAX];
	char            oid[GIT_OID_SHA1_HEXSIZE + 1], configoid[GIT_OID_SHA1_HEXSIZE + 1];
	struct stat     st;
	struct archive* a;

	emkdirf("!%s/.cache/archives", info->destdir);

	git_oid_tostr(oid, sizeof(oid), git_commit_id(refinfo->commit));

	snprintf(path, sizeof(path), "%s/%s/%s.%s", info->destdir, refinfo->refname, refinfo->refname,
	         archiveexts[type]);

	if (!force &&
	    !loadbuffer(configoid, GIT_OID_SHA1_HEXSIZE, "%s/.cache/archives/%s", info->destdir,
	                refinfo->refname) &&
	    !access(path, R_OK)) {
		configoid[GIT_OID_SHA1_HEXSIZE] = '\0';
		if (!strcmp(configoid, oid))
			goto getsize;
	}

	/* Get the tree associated with the commit */
	if (git_commit_tree(&tree, refinfo->commit)) {
		hprintf(stderr, "error: unable to get tree from commit: %gw\n");
		git_commit_free(refinfo->commit);
		return -1;
	}

	a = archive_write_new();
	switch (type) {
		case ArchiveTarGz:
			archive_write_add_filter_gzip(a);
			archive_write_set_format_pax_restricted(a);
			break;
		case ArchiveTarXz:
			archive_write_add_filter_lzma(a);
			archive_write_set_format_pax_restricted(a);
			break;
		case ArchiveTarBz2:
			archive_write_add_filter_bzip2(a);
			archive_write_set_format_pax_restricted(a);
			break;
		case ArchiveZip:
			archive_write_set_format_zip(a);
			break;
		default:
			fprintf(stderr, "error: invalid type passed to archiver\n");
			return -1;
	}

	if (archive_write_open_FILE(a, fp) != ARCHIVE_OK) {
		fprintf(stderr, "error: unable to open archive: %s\n", archive_error_string(a));
		git_tree_free(tree);
		return -1;
	}

	/* Process the tree to archive it */
	if (walktree(info->repo, tree, "", a) != 0) {
		hprintf(stderr, "error: unable to process tree: %gw\n");
		git_tree_free(tree);
		archive_write_close(a);
		archive_write_free(a);
		return -1;
	}

	writebuffer(oid, GIT_OID_SHA1_HEXSIZE, "%s/.cache/archives/%s", info->destdir,
	            refinfo->refname);

	git_tree_free(tree);
	archive_write_close(a);
	archive_write_free(a);

getsize:
	if (stat(path, &st)) {
		hprintf(stderr, "error: unable to stat %s: %w\n", path);
		return -1;
	}

	return st.st_size;
}
