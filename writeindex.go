package gitin

import (
	"fmt"
	"html"
	"io"
	"os"
	"path"
)

func writeindexline(fp, cachefp io.Writer, ref *referenceinfo, repodir, name, description string) int {

	var ret int = 0

	fmt.Fprintf(fp, "<tr><td><a href=\"%s/\">%s</a></td><td>%s</td><td>", repodir, html.EscapeString(name), html.EscapeString(description))
	if ref != nil {
		fmt.Fprint(fp, rfc3339(ref.commit.Author().When))
		sig := ref.commit.Author()
		if sig != nil && sig.Name != "" {
			fmt.Fprintf(fp, " by %s", sig.Name)
		}
	}
	fmt.Fprintf(fp, "</td></tr>")

	fmt.Fprintf(cachefp, "%s,%s,%s\n", repodir, name, description)

	return ret
}

func writecategory(index io.Writer, name string) error {
	var category = name
	var keys struct {
		des string `conf:"description"`
	}

	if file, err := os.Open(path.Join(category, Config.Configfile)); err == nil {
		defer file.Close()
		for _, value := range ParseConfig(file, path.Join(category, Config.Configfile)) {
			if err := UnmarshalConf(value, "", &keys); err != nil {
				return err
			}
		}
	}

	fmt.Fprintf(index, "<tr class=\"category\"><td>%s</td><td>%s</td><td></td></tr>\n", html.EscapeString(category), html.EscapeString(keys.des))
	return nil
}

func iscategory(repodir string, category *string) bool {
	cur := path.Dir(repodir)
	if *category != cur {
		*category = cur
		return true
	}
	return false
}

func WriteIndex(fp io.Writer, info []indexinfo) error {
	os.MkdirAll(".cache", 0777)

	cachefp, err := os.Create(".cache/index")
	if err != nil {
		return err
	}
	writeheader(fp, nil, 0, false, Config.Sitename, html.EscapeString(Config.Sitedescription))
	fmt.Fprintf(fp, "<table id=\"index\"><thead>\n"+
		"<tr><td>Name</td><td class=\"expand\">Description</td><td>Last changes</td></tr>"+
		"</thead><tbody>\n")

	var category string = ""
	for _, index := range info {
		if iscategory(index.Repodir, &category) {
			if err := writecategory(fp, category); err != nil {
				return err
			}
		}
		if index.Name == "" {
			repoinfo, err := Getrepo(index.Repodir, 0)
			if err != nil {
				return err
			}
			writeindexline(fp, cachefp, repoinfo.branch, index.Repodir, repoinfo.name,
				repoinfo.description)
		} else {
			writeindexline(fp, cachefp, nil, index.Repodir, index.Name,
				index.Description)
		}
	}
	fmt.Fprintf(fp, "</tbody>\n</table>")
	writefooter(fp)
	return nil
}
