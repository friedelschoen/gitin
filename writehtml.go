package gitin

import (
	"fmt"
	"html"
	"io"
	"strings"
)

func writeheader(fp io.Writer, info *repoinfo, relpath int, inbranch bool, name, description string) {
	var indexrelpath int = relpath
	if info != nil {
		indexrelpath += info.relpath
	}

	indexrelstr := strings.Repeat("../", indexrelpath)

	fmt.Fprintf(fp, "<!DOCTYPE html>\n"+
		"<html>\n<head>\n"+
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"+
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />")
	fmt.Fprintf(fp, "<title>%s - %s</title>\n", html.EscapeString(name), html.EscapeString(config.Sitename))
	fmt.Fprintf(fp, "<link rel=\"icon\" type=\"%s\" href=\"%s%s\" />\n", config.Favicontype, indexrelstr, config.Favicon)
	fmt.Fprintf(fp, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s%s\" />\n", indexrelstr, config.Stylesheet)
	fmt.Fprint(fp, "</head>\n<body>\n<table><tr><td>")
	fmt.Fprintf(fp, "<a href=\"%s\"><img src=\"%s%s\" alt=\"\" width=\"50\" height=\"50\" /></a>", indexrelstr, indexrelstr, config.Logoicon)
	fmt.Fprintf(fp, "</td><td class=\"expand\"><h1>%s</h1>\n<span class=\"desc\">%s</span></td></tr>", html.EscapeString(name), html.EscapeString(description))

	if info != nil && info.cloneurl != "" {
		fmt.Fprintf(fp, "<tr class=\"url\"><td></td><td class=\"expand\"><code>git clone <a href=\"%s\">%s</a></code></td></tr>", info.cloneurl, html.EscapeString(info.cloneurl))
	}
	fmt.Fprintf(fp, "<tr><td></td><td class=\"expand\">\n")
	if info != nil {
		fmt.Fprintf(fp, "<a href=\"%s\">Repositories</a> ", indexrelstr)
		if inbranch {
			fmt.Fprintf(fp, "| <a href=\"%sindex.html\">Summary</a> ", strings.Repeat("../", relpath-1))
			fmt.Fprintf(fp, "| <a href=\"%slog.html\">Log</a> ", strings.Repeat("../", relpath-1))
			fmt.Fprintf(fp, "| <a href=\"%sfiles/\">Files</a> ", strings.Repeat("../", relpath-1))
		}
		if info.submodules != "" {
			fmt.Fprintf(fp, " | <a href=\"%s%s/files/%s.html\">Submodules</a>", strings.Repeat("../", relpath), info.branch.refname, info.submodules)
		}
		for _, pf := range info.pinfiles {
			fmt.Fprintf(fp, " | <a href=\"%s%s/files/%s.html\">%s</a>", strings.Repeat("../", relpath), info.branch.refname, pf, pf)
		}
	} else {
		fmt.Fprintf(fp, "</td><td>")
	}
	fmt.Fprintf(fp, "</td></tr></table>\n<hr/>\n<div id=\"content\">\n")
}

func writefooter(fp io.Writer) {
	if config.Footertext != "" {
		fmt.Fprintf(fp, "</div><div id=\"footer\">%s", config.Footertext)
	}
	fmt.Fprintf(fp, "</div>\n</body>\n</html>\n")
}
