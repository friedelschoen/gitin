package render

import (
	"fmt"
	"html"
	"io"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
)

func WriteHeader(fp io.Writer, info *wrapper.RepoInfo, relpath int, inbranch bool, name, description string) {
	var indexrelpath int = relpath
	if info != nil {
		indexrelpath += info.Relpath
	}

	indexrelstr := common.Relpath(indexrelpath)

	fmt.Fprintf(fp, "<!DOCTYPE html>\n"+
		"<html>\n<head>\n"+
		"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"+
		"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />")
	fmt.Fprintf(fp, "<title>%s - %s</title>\n", html.EscapeString(name), html.EscapeString(gitin.Config.Sitename))
	fmt.Fprintf(fp, "<link rel=\"icon\" type=\"%s\" href=\"%s%s\" />\n", gitin.Config.Favicontype, indexrelstr, gitin.Config.Favicon)
	fmt.Fprintf(fp, "<link rel=\"stylesheet\" type=\"text/css\" href=\"%s%s\" />\n", indexrelstr, gitin.Config.Stylesheet)
	fmt.Fprint(fp, "</head>\n<body>\n<table><tr><td>")
	fmt.Fprintf(fp, "<a href=\"%s\"><img src=\"%s%s\" alt=\"\" width=\"50\" height=\"50\" /></a>", indexrelstr, indexrelstr, gitin.Config.Logoicon)
	fmt.Fprintf(fp, "</td><td class=\"expand\"><h1>%s</h1>\n<span class=\"desc\">%s</span></td></tr>", html.EscapeString(name), html.EscapeString(description))

	if info != nil && info.Cloneurl != "" {
		fmt.Fprintf(fp, "<tr class=\"url\"><td></td><td class=\"expand\"><code>git clone <a href=\"%s\">%s</a></code></td></tr>", info.Cloneurl, html.EscapeString(info.Cloneurl))
	}
	fmt.Fprintf(fp, "<tr><td></td><td class=\"expand\">\n")
	if info != nil {
		fmt.Fprintf(fp, "<a href=\"%s\">Repositories</a> ", indexrelstr)
		if inbranch {
			fmt.Fprintf(fp, "| <a href=\"%sindex.html\">Summary</a> ", common.Relpath(relpath-1))
			fmt.Fprintf(fp, "| <a href=\"%slog.html\">Log</a> ", common.Relpath(relpath-1))
			fmt.Fprintf(fp, "| <a href=\"%sfiles/\">Files</a> ", common.Relpath(relpath-1))
		}
		if info.Submodules != "" {
			fmt.Fprintf(fp, " | <a href=\"%s%s/files/%s.html\">Submodules</a>", common.Relpath(relpath), info.Branch.Refname, info.Submodules)
		}
		for _, pf := range info.Pinfiles {
			fmt.Fprintf(fp, " | <a href=\"%s%s/files/%s.html\">%s</a>", common.Relpath(relpath), info.Branch.Refname, pf, pf)
		}
	} else {
		fmt.Fprintf(fp, "</td><td>")
	}
	fmt.Fprintf(fp, "</td></tr></table>\n<hr/>\n<div id=\"content\">\n")
}

func WriteFooter(fp io.Writer) {
	if gitin.Config.Footertext != "" {
		fmt.Fprintf(fp, "</div><div id=\"footer\">%s", gitin.Config.Footertext)
	}
	fmt.Fprintf(fp, "</div>\n</body>\n</html>\n")
}
