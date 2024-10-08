#include "gitin.h"

#include <stdarg.h>


void writeheader(FILE* fp, const struct repoinfo* info, int relpath, const char* name,
                 const char* description, ...) {
	va_list desc_args;
	int     indexrelpath = relpath + (info ? info->relpath : 0);

	fputs("<!DOCTYPE html>\n"
	      "<html>\n<head>\n"
	      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />",
	      fp);
	hprintf(fp, "<title>%y - %y</title>\n", name, sitename);
	hprintf(fp, "<link rel=\"icon\" type=\"%s\" href=\"%r%s\" />\n", favicontype, indexrelpath,
	        favicon);
	hprintf(fp, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%r%s\" />\n", indexrelpath,
	        stylesheet);
	fputs("</head>\n<body>\n<table><tr><td>", fp);
	hprintf(fp, "<a href=\"%r\"><img src=\"%r%s\" alt=\"\" width=\"50\" height=\"50\" /></a>",
	        indexrelpath, indexrelpath, logoicon);
	hprintf(fp, "</td><td class=\"expand\"><h1>%y</h1>\n<span class=\"desc\">", name);

	va_start(desc_args, description);
	vhprintf(fp, description, desc_args);
	va_end(desc_args);
	fputs("</span></td></tr>", fp);
	if (info && *info->cloneurl) {
		hprintf(
		    fp,
		    "<tr class=\"url\"><td></td><td class=\"expand\"><code>git clone <a href=\"%s\">%y</a></code></td></tr>",
		    info->cloneurl, info->cloneurl);
	}
	fputs("<tr><td></td><td class=\"expand\">\n", fp);
	if (info) {
		hprintf(fp, "<a href=\"%r\">Log</a>", relpath);
		hprintf(fp, " | <a href=\"%rfile/\">Files</a> ", relpath);
		if (info->submodules)
			hprintf(fp, " | <a href=\"%rfile/%s.html\">Submodules</a>", relpath, info->submodules);
		for (int i = 0; i < info->pinfileslen; i++)
			hprintf(fp, " | <a href=\"%rfile/%s.html\">%s</a>", relpath, info->pinfiles[i],
			        info->pinfiles[i]);
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
