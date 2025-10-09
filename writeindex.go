package gitin

import (
	"fmt"
	"html"
	"io"
	"os"
	"path"
)

// #include "common.h"
// #include "config.h"
// #include "getinfo.h"
// #include "fmt.Fprintf.h"
// #include "writer.h"

// #include <git2/commit.h>
// #include <git2/refs.h>
// #include <git2/repository.h>
// #include <git2/types.h>
// #include <limits.h>
// #include <stdio.h>
// #include <string.h>

func writeindexline(fp, cachefp io.Writer, ref *referenceinfo, repodir, name, description string) int { // static int writeindexline(io.Writer fp, io.Writer cachefp, struct referenceinfo* ref, const char* repodir,
	// const char* name, const char* description) {
	var ret int = 0 // int ret = 0;

	// TODO: get time of HEAD
	fmt.Fprintf(fp, "<tr><td><a href=\"%s/\">%s</a></td><td>%s</td><td>", repodir, html.EscapeString(name), html.EscapeString(description)) // fmt.Fprintf(fp, "<tr><td><a href=\"%s/\">%y</a></td><td>%y</td><td>", repodir, name, description);
	if ref != nil {                                                                                                                         // if (ref) {
		fmt.Fprint(fp, rfc3339(ref.commit.Author().When)) // fmt.Fprintf(fp, "%t", git.Commit_time(ref->commit));
		sig := ref.commit.Author()                        // const git.Signature* sig = git.Commit_author(ref->commit);
		if sig != nil && sig.Name != "" {                 // if (sig && sig->name)
			fmt.Fprintf(fp, " by %s", sig.Name) // fmt.Fprintf(fp, " by %s", sig->name);
		}
	}
	fmt.Fprintf(fp, "</td></tr>") // fmt.Fprintf("</td></tr>", fp);

	fmt.Fprintf(cachefp, "%s,%s,%s\n", repodir, name, description) // fmt.Fprintf(cachefp, "%s,%s,%s\n", repodir, name, description);

	return ret // return ret;
}

func writecategory(index io.Writer, name string) error { // static void writecategory(io.Writer index, const char* name, int len) {
	var category = name // char  category[PATH_MAX];
	var keys struct {
		des string `conf:"description"`
	}

	if file, err := os.Open(path.Join(category, config.Configfile)); err == nil { // if ((fp = efopen("!r", "%s/%s", category, configfile))) {
		defer file.Close()
		for _, value := range ParseConfig(file, path.Join(category, config.Configfile)) {
			if err := UnmarshalConf(value, "", &keys); err != nil {
				return err
			}
		}
	}

	fmt.Fprintf(index, "<tr class=\"category\"><td>%s</td><td>%s</td><td></td></tr>\n", html.EscapeString(category), html.EscapeString(keys.des)) // description);
	return nil
}

func iscategory(repodir string, category *string) bool { // static int iscategory(const char* repodir, const char** category, int* categorylen) {
	cur := path.Dir(repodir)
	if *category != cur {
		*category = cur
		return true
	}
	return false
}

func writeindex(fp io.Writer, info *gitininfo) error { // void writeindex(io.Writer fp, const struct gitininfo* info) {
	os.MkdirAll(".cache", 0777)

	cachefp, err := os.Create(".cache/index")
	if err != nil {
		return err
	}
	writeheader(fp, nil, 0, false, config.Sitename, html.EscapeString(config.Sitedescription)) // writeheader(fp, NULL, 0, 0, sitename, "%y", sitedescription);
	fmt.Fprintf(fp, "<table id=\"index\"><thead>\n"+
		"<tr><td>Name</td><td class=\"expand\">Description</td><td>Last changes</td></tr>"+
		"</thead><tbody>\n") // fp);

	var category string = ""             // const char*     category    = NULL;
	for i, index := range info.indexes { // for (int i = 0; i < info->nindexes; i++) {
		if iscategory(index.repodir, &category) { // if (iscategory(info->indexes[i].repodir, &category, &categorylen))
			if err := writecategory(fp, category); err != nil { // writecategory(fp, category, categorylen);
				return err
			}
		}
		if index.name == "" { // if (!info->indexes[i].name) {
			repoinfo, err := getrepo(info.indexes[i].repodir, 0) // getrepo(&repoinfo, info->indexes[i].repodir, 0);
			if err != nil {
				return err
			}
			writeindexline(fp, cachefp, repoinfo.branch, info.indexes[i].repodir, repoinfo.name, // writeindexline(fp, cachefp, &repoinfo.branch, info->indexes[i].repodir, repoinfo.name,
				repoinfo.description) // repoinfo.description);
		} else {
			writeindexline(fp, cachefp, nil, info.indexes[i].repodir, info.indexes[i].name, // writeindexline(fp, cachefp, NULL, info->indexes[i].repodir, info->indexes[i].name,
				info.indexes[i].description) // info->indexes[i].description);
		}
	}
	fmt.Fprintf(fp, "</tbody>\n</table>") // fmt.Fprintf("</tbody>\n</table>", fp);
	writefooter(fp)                       // writefooter(fp);
	return nil
}
