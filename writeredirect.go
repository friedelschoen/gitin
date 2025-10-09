package gitin

import (
	"fmt"
	"io"
)

func writeredirect(fp io.Writer, to string) {
	fmt.Fprintf(fp,
		"<!DOCTYPE HTML>\n"+
			"<html>\n"+
			"    <head>\n"+
			"        <meta charset=\"UTF-8\" />\n"+
			"        <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"+
			"        <meta http-equiv=\"Refresh\" content=\"0; url=%s\" />\n"+
			"        <title>Redirect</title>\n"+
			"    </head>\n"+
			"    <body>\n"+
			"        If you are not redirected automatically, follow this <a href=\"%s\">link</a>.\n"+
			"    </body>\n"+
			"</html>",
		to, to)
}
