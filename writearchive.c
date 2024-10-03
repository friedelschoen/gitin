#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "writer.h"

#include <archive.h>
#include <archive_entry.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>

// Function to write a blob (file) from the repository to the archive
static int write_blob_to_archive(git_blob* blob, const char* path, const git_tree_entry* gitentry,
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

// Recursively process the tree to archive files
static int process_tree(git_repository* repo, git_tree* tree, const char* base_path,
                        struct archive* a) {
	size_t count = git_tree_entrycount(tree);
	for (size_t i = 0; i < count; ++i) {
		const git_tree_entry* entry = git_tree_entry_byindex(tree, i);
		const char*           name  = git_tree_entry_name(entry);
		char                  full_path[1024];
		snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);

		if (git_tree_entry_type(entry) == GIT_OBJ_TREE) {
			git_tree* subtree;
			if (git_tree_entry_to_object((git_object**) &subtree, repo, entry) != 0) {
				hprintf(stderr, "error: unable to load git-tree: %gw\n");
				return -1;
			}
			process_tree(repo, subtree, full_path, a);
			git_tree_free(subtree);
		} else if (git_tree_entry_type(entry) == GIT_OBJ_BLOB) {
			git_blob* blob;
			if (git_tree_entry_to_object((git_object**) &blob, repo, entry) != 0) {
				hprintf(stderr, "error: unable to load blob: %gw\n");
				return -1;
			}
			if (write_blob_to_archive(blob, full_path, entry, a) != 0) {
				git_blob_free(blob);
				return -1;
			}
			git_blob_free(blob);
		}
	}
	return 0;
}

// Updated function to accept git_reference instead of branch/tag name
int writearchive(const struct repoinfo* info, const struct git_reference* ref) {
	git_commit* commit = NULL;
	git_tree*   tree   = NULL;
	int         error  = 0;
	char        path[PATH_MAX];
	char*       dir;
	FILE*       fp;
	char        oid[GIT_OID_HEXSZ + 1], configoid[GIT_OID_HEXSZ + 1];

	// Get the commit the reference points to
	error = git_reference_peel((git_object**) &commit, ref, GIT_OBJECT_COMMIT);
	if (error != 0) {
		hprintf(stderr, "error: unable to peel reference to commit: %gw\n");
		return -1;
	}

	git_oid_tostr(oid, sizeof(oid), git_commit_id(commit));

	if (!force) {
		snprintf(path, sizeof(path), "%s/.gitin/archive/%s", info->destdir,
		         git_reference_shorthand(ref));
		if ((fp = fopen(path, "r"))) {
			fread(configoid, GIT_OID_HEXSZ, 1, fp);
			configoid[GIT_OID_HEXSZ] = '\0';
			fclose(fp);

			if (!strcmp(oid, configoid)) {
				git_commit_free(commit);
				return 0;
			}
		}
	}

	// Get the tree associated with the commit
	error = git_commit_tree(&tree, commit);
	if (error != 0) {
		hprintf(stderr, "error: unable to get tree from commit: %gw\n");
		git_commit_free(commit);
		return -1;
	}

	snprintf(path, sizeof(path), "%s/archive/%s.tar.gz", info->destdir,
	         git_reference_shorthand(ref));
	dir = dirname(path);
	if (mkdirp(dir, 0777) != 0) {
		hprintf(stderr, "error: unable to create directory: %w\n");
		git_tree_free(tree);
		git_commit_free(commit);
		return -1;
	}

	snprintf(path, sizeof(path), "%s/archive/%s.tar.gz", info->destdir,
	         git_reference_shorthand(ref));

	struct archive* a;
	a = archive_write_new();
	archive_write_add_filter_gzip(a);
	archive_write_set_format_pax_restricted(a);
	if (archive_write_open_filename(a, path) != ARCHIVE_OK) {
		fprintf(stderr, "error: unable to open archive: %s\n", archive_error_string(a));
		git_tree_free(tree);
		git_commit_free(commit);
		return -1;
	}

	// Process the tree to archive it
	if (process_tree(info->repo, tree, "", a) != 0) {
		hprintf(stderr, "error: unable to process tree: %gw\n");
		git_tree_free(tree);
		git_commit_free(commit);
		archive_write_close(a);
		archive_write_free(a);
		return -1;
	}

	snprintf(path, sizeof(path), "%s/.gitin/archive/%s", info->destdir,
	         git_reference_shorthand(ref));
	if ((fp = fopen(path, "w"))) {
		if (verbose)
			fprintf(stderr, "%s\n", path);
		fwrite(oid, GIT_OID_HEXSZ, 1, fp);
		fclose(fp);
	} else {
		hprintf(stderr, "error: unable to write oid to file: %w\n");
		git_tree_free(tree);
		git_commit_free(commit);
		archive_write_close(a);
		archive_write_free(a);
		return -1;
	}

	git_tree_free(tree);
	git_commit_free(commit);
	archive_write_close(a);
	archive_write_free(a);
	return 0;
}
