#include "common.h"
#include "config.h"
#include "hprintf.h"
#include "writer.h"

void writeheader(FILE* fp, const struct repoinfo* info, int relpath, const char* name, const char* description) {
	int indexrelpath = relpath + (info ? info->relpath : 0);
	fputs("<!DOCTYPE html>\n"
	      "<html>\n<head>\n"
	      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />",
	      fp);
	hprintf(fp, "<title>%y - %y</title>\n", name, sitename);
	hprintf(fp, "<link rel=\"icon\" type=\"%s\" href=\"%r%s\" />\n", favicontype, indexrelpath, favicon);
	hprintf(fp, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%r%s\" />\n", indexrelpath, stylesheet);
	fputs("</head>\n<body>\n<table><tr><td>", fp);
	hprintf(fp, "<a href=\"%r\"><img src=\"%r%s\" alt=\"\" width=\"32\" height=\"32\" /></a>", indexrelpath,
	        indexrelpath, logoicon);
	hprintf(fp, "</td><td><h1>%y</h1>\n<span class=\"desc\">%y</span>", name, description);
	fputs("</td></tr>", fp);
	if (info && *info->cloneurl) {
		hprintf(fp, "<tr class=\"url\"><td></td><td>git clone <a href=\"%s\">%y</a></td></tr>", info->cloneurl,
		        info->cloneurl);
	}
	fputs("<tr><td></td><td>\n", fp);
	if (info) {
		hprintf(fp, "<a href=\"%rlog.html\">Log</a> | ", relpath);
		hprintf(fp, "<a href=\"%rfiles.html\">Files</a> | ", relpath);
		hprintf(fp, "<a href=\"%rrefs.html\">Refs</a>", relpath);
		if (info->submodules)
			hprintf(fp, " | <a href=\"%rfile/%s.html\">Submodules</a>", relpath, info->submodules);
		for (int i = 0; i < info->pinfileslen; i++)
			hprintf(fp, " | <a href=\"%rfile/%s.html\">%s</a>", relpath, info->pinfiles[i], info->pinfiles[i]);
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
