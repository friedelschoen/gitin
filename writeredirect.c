#include "writer.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

void writeredirect(FILE* fp, const char* to, ...) {
	char    path[PATH_MAX];
	va_list args;

	va_start(args, to);
	vsnprintf(path, sizeof(path), to, args);
	va_end(args);

	fprintf(
	    fp,
	    "<!DOCTYPE HTML>\n"
	    "<html>\n"
	    "    <head>\n"
	    "        <meta charset=\"UTF-8\" />\n"
	    "        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	    "        <meta http-equiv=\"Refresh\" content=\"0; url=%s\" />\n"
	    "        <title>Redirect</title>\n"
	    "    </head>\n"
	    "    <body>\n"
	    "        If you are not redirected automatically, follow this <a href=\"%s\">link</a>.\n"
	    "    </body>\n"
	    "</html>",
	    path, path);
}
