#include "gitin.h"

#include <string.h>

static ssize_t writepandoc(FILE* fp, const struct repoinfo* info, const char* filename,
                           const char* content, ssize_t len, uint32_t contenthash,
                           const char* type) {

	ssize_t                 ret;
	struct callcached_param params = {
		.command     = pandoccmd,
		.cachename   = "preview",
		.content     = content,
		.ncontent    = len,
		.contenthash = contenthash,
		.fp          = fp,
		.info        = info,
		.nenviron    = 2,
		.environ     = (const char*[]){ "filename", filename, "type", type },
	};

	fprintf(fp, "<div id=\"preview\">\n");
	ret = callcached(&params);
	fprintf(fp, "</div>\n");

	return ret;
}

static ssize_t writetree(FILE* fp, const struct repoinfo* info, const char* content, ssize_t len,
                         uint32_t contenthash, const char* type) {

	ssize_t                 ret;
	struct callcached_param params = {
		.command     = configtreecmd,
		.cachename   = "preview",
		.content     = content,
		.ncontent    = len,
		.contenthash = contenthash,
		.fp          = fp,
		.info        = info,
		.nenviron    = 1,
		.environ     = (const char*[]){ "type", type },
	};

	fprintf(fp, "<div id=\"preview\">\n");
	ret = callcached(&params);
	fprintf(fp, "</div>\n");

	return ret;
}

static void writeimage(FILE* fp, int relpath, const char* filename) {
	fprintf(fp, "<div id=\"preview\">\n");
	hprintf(fp, "<img height=\"100px\" src=\"%rblob/%s\" />\n", relpath, filename);
	fprintf(fp, "</div>\n");
}


void writepreview(FILE* fp, const struct repoinfo* info, int relpath, const char* filename,
                  const char* content, ssize_t len, uint32_t contenthash) {
	char type[1024] = "", *param;

	for (int i = 0; filetypes[i][0] != NULL; i++) {
		if (endswith(filename, filetypes[i][0])) {
			strlcpy(type, filetypes[i][2], sizeof(type));
			break;
		}
	}

	if (!*type)
		return;

	if ((param = strchr(type, ':')))
		*param++ = '\0';

	if (!strcmp(type, "pandoc")) {
		if (!param && (param = strchr(filename, '.'))) {
			param++;
		}

		if (param)
			writepandoc(fp, info, filename, content, len, contenthash, param);
	} else if (!strcmp(type, "configtree")) {
		if (!param && (param = strchr(filename, '.'))) {
			param++;
		}

		if (param)
			writetree(fp, info, content, len, contenthash, param);
	} else if (!strcmp(type, "image")) {
		writeimage(fp, relpath, filename);
	}
}
