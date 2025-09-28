#include "common.h"
#include "config.h"
#include "execute.h"
#include "hprintf.h"
#include "writer.h"

#include <git2/blob.h>
#include <string.h>


static void xmlencode(FILE* fp, const char* s, size_t size) {
	while (*s && --size > 0) {
		switch (*s) {
			case '<':
				fputs("&lt;", fp);
				break;
			case '>':
				fputs("&gt;", fp);
				break;
			case '\'':
				fputs("&#39;", fp);
				break;
			case '&':
				fputs("&amp;", fp);
				break;
			case '"':
				fputs("&quot;", fp);
				break;
			default:
				putc(*s, fp);
		}
		s++;
	}
}

static ssize_t writepandoc(FILE* fp, const char* filename, const char* content, ssize_t len,
                           uint32_t contenthash, const char* type) {

	ssize_t            ret;
	struct executeinfo params = {
		.command     = pandoccmd,
		.cachename   = "preview",
		.content     = content,
		.ncontent    = len,
		.contenthash = contenthash,
		.fp          = fp,
		.nenviron    = 2,
		.environ     = (const char*[]){ "filename", filename, "type", type },
	};

	fprintf(fp, "<div class=\"preview\">\n");
	ret = execute(&params);
	fprintf(fp, "</div>\n");

	return ret;
}

static ssize_t writetree(FILE* fp, const char* content, ssize_t len, uint32_t contenthash,
                         const char* type) {

	ssize_t            ret;
	struct executeinfo params = {
		.command     = configtreecmd,
		.cachename   = "preview",
		.content     = content,
		.ncontent    = len,
		.contenthash = contenthash,
		.fp          = fp,
		.nenviron    = 1,
		.environ     = (const char*[]){ "type", type },
	};

	fprintf(fp, "<div class=\"preview\">\n");
	ret = execute(&params);
	fprintf(fp, "</div>\n");

	return ret;
}

static void writeimage(FILE* fp, int relpath, const char* filename) {
	fprintf(fp, "<div class=\"preview\">\n");
	hprintf(fp, "<img height=\"100px\" src=\"%rblob/%s\" />\n", relpath, filename);
	fprintf(fp, "</div>\n");
}

static void writeplain(FILE* fp, const char* content, size_t size) {
	fprintf(fp, "<div class=\"preview\">\n<pre>\n");
	xmlencode(fp, content, size);
	fprintf(fp, "</pre>\n</div>\n");
}

void writepreview(FILE* fp, int relpath, struct blobinfo* blob, int printplain) {
	char type[1024] = "", *param;

	for (int i = 0; filetypes[i][0] != NULL; i++) {
		if (issuffix(blob->name, filetypes[i][0])) {
			strlcpy(type, filetypes[i][2], sizeof(type));
			break;
		}
	}

	if ((param = strchr(type, ':')))
		*param++ = '\0';

	if (!strcmp(type, "pandoc")) {
		if (!param && (param = strchr(blob->name, '.')))
			param++;

		if (param)
			writepandoc(fp, blob->name, git_blob_rawcontent(blob->blob),
			            git_blob_rawsize(blob->blob), blob->hash, param);
	} else if (!strcmp(type, "configtree")) {
		if (!param && (param = strchr(blob->name, '.')))
			param++;

		if (param)
			writetree(fp, git_blob_rawcontent(blob->blob), git_blob_rawsize(blob->blob), blob->hash,
			          param);
	} else if (!strcmp(type, "image")) {
		writeimage(fp, relpath, blob->name);
	} else if (printplain && !git_blob_is_binary(blob->blob)) {
		writeplain(fp, git_blob_rawcontent(blob->blob), git_blob_rawsize(blob->blob));
	}
}
