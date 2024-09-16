#include "common.h"
#include "config.h"
#include "writer.h"

void writeheader(FILE* fp, const struct repoinfo* info, const char* indexrelpath, const char* relpath, const char* name,
                 const char* description) {

	fputs("<!DOCTYPE html>\n"
	      "<html>\n<head>\n"
	      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
	      "<title>",
	      fp);
	xmlencode(fp, name);
	fputs(" - ", fp);
	xmlencode(fp, sitename);
	fprintf(fp, "</title>\n<link rel=\"icon\" type=\"%s\" href=\"%s%s\" />\n", relpath, favicontype, favicon);
	fprintf(fp, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s%s\" />\n", relpath, stylesheet);
	fputs("</head>\n<body>\n<table><tr><td>", fp);
	fprintf(fp, "<a href=\"%s\"><img src=\"%s%s\" alt=\"\" width=\"32\" height=\"32\" /></a>", relpath, relpath,
	        logoicon);
	fputs("</td><td><h1>", fp);
	xmlencode(fp, name);
	fputs("</h1><span class=\"desc\">", fp);
	xmlencode(fp, description);
	fputs("</span></td></tr>", fp);
	if (info && *info->cloneurl) {
		fprintf(fp, "<tr class=\"url\"><td></td><td>git clone <a href=\"%s\">", info->cloneurl);
		xmlencode(fp, info->cloneurl);
		fputs("</a></td></tr>", fp);
	}
	fputs("<tr><td></td><td>\n", fp);
	if (info) {
		fprintf(fp, "<a href=\"%slog.html\">Log</a> | ", relpath);
		fprintf(fp, "<a href=\"%sfiles.html\">Files</a> | ", relpath);
		fprintf(fp, "<a href=\"%srefs.html\">Refs</a>", relpath);
		if (info->submodules)
			fprintf(fp, " | <a href=\"%sfile/%s.html\">Submodules</a>", relpath, info->submodules);
		for (int i = 0; i < info->pinfileslen; i++)
			fprintf(fp, " | <a href=\"%sfile/%s.html\">%s</a>", relpath, info->pinfiles[i], info->pinfiles[i]);
	} else {
		fputs("</td><td>", fp);
	}
	fputs("</td></tr></table>\n<hr/>\n<div id=\"content\">\n", fp);
}


void writefooter(FILE* fp) {
	if (footertext) {
		fprintf(fp, "</div><div id=\"footer\">%s", footertext);
	}
	fputs("</div>\n</body>\n</html>\n", fp);
}
