package gitin

import (
	"errors"
	"fmt"
	"html"
	"io"
	"os"
	"path"

	git "github.com/jeffwelling/git2go/v37"
)

// #include "common.h"
// #include "config.h"
// #include "getinfo.h"
// #include "fmt.Fprintf.h"
// #include "writer.h"

// #include <git2/commit.h>
// #include <git2/revwalk.h>
// #include <limits.h>
// #include <unistd.h>

func writelogline(fp io.Writer, commit *git.Commit, ci *commitinfo) { // static void writelogline(io.Writer fp, git.Commit* commit, const struct commitinfo* ci) {
	author := commit.Author()   // const git.Signature* author  = git.Commit_author(commit);
	summary := commit.Summary() // const char*          summary = git.Commit_summary(commit);

	oid := commit.Id().String() // git.Oid_tostr(oid, sizeof(oid), git.Commit_id(commit));

	fmt.Fprint(fp, "<tr><td>") // fmt.Fprintf("<tr><td>", fp);
	if author != nil {         // if (author)
		fmt.Fprint(fp, rfc3339(author.When)) // fmt.Fprintf(fp, "%t", author->when);
	}
	fmt.Fprint(fp, "</td><td>") // fmt.Fprintf("</td><td>", fp);
	if summary != "" {          // if (summary) {
		fmt.Fprintf(fp, "<a href=\"../commit/%s.html\">%s</a>", oid, html.EscapeString(summary)) // fmt.Fprintf(fp, "<a href=\"../commit/%s.html\">%y</a>", oid, summary);
	}
	fmt.Fprintf(fp, "</td><td>") // fmt.Fprintf("</td><td>", fp);
	if author != nil {           // if (author)
		fmt.Fprintf(fp, "%s", html.EscapeString(author.Name)) // fmt.Fprintf(fp, "%y", author->name);
	}
	fmt.Fprintf(fp, "</td><td class=\"num\" align=\"right\">") // fmt.Fprintf("</td><td class=\"num\" align=\"right\">", fp);
	fmt.Fprintf(fp, "%d", len(ci.Deltas))                      // fmt.Fprintf(fp, "%zu", ci->ndeltas);
	fmt.Fprintf(fp, "</td><td class=\"num\" align=\"right\">") // fmt.Fprintf("</td><td class=\"num\" align=\"right\">", fp);
	fmt.Fprintf(fp, "+%d", ci.Addcount)                        // fmt.Fprintf(fp, "+%zu", ci->addcount);
	fmt.Fprintf(fp, "</td><td class=\"num\" align=\"right\">") // fmt.Fprintf("</td><td class=\"num\" align=\"right\">", fp);
	fmt.Fprintf(fp, "-%d", ci.Delcount)                        // fmt.Fprintf(fp, "-%zu", ci->delcount);
	fmt.Fprint(fp, "</td></tr>\n")                             // fmt.Fprintf("</td></tr>\n", fp);
}

func FileExist(pat string) bool {
	_, err := os.Stat(pat)
	return !errors.Is(err, os.ErrNotExist)
}

type logstate struct {
	fp   io.Writer
	json []JSONCommit
	atom []atomEntry
}

func writelogcommit(s *logstate, info *repoinfo, index int, commit *git.Commit) error { // static void writelogcommit(io.Writer fp, io.Writer json, io.Writer atom, const struct repoinfo* info, int index,
	pat := path.Join("commit", commit.Id().String()+".html")
	cachedcommit := !force && FileExist(pat) // cachedcommit = !force && !access(path, F_OK);
	ci, err := getdiff(info, commit, cachedcommit)
	if err != nil { // if (getdiff(&ci, info, commit, cachedcommit) == -1)
		return err // return;
	}

	if !cachedcommit { // if (!cachedcommit) {
		commitfile, err := os.Create(pat)
		if err != nil {
			return err
		}
		defer commitfile.Close()
		if err := writecommit(commitfile, info, commit, &ci, index == config.Maxcommits); err != nil { // writecommit(commitfile, info, commit, &ci, index == maxcommits);
			return err
		}
	}
	if s.fp != nil { // if (fp)
		writelogline(s.fp, commit, &ci) // writelogline(fp, commit, &ci);
	}
	s.atom = append(s.atom, makeEntry(commit, ""))
	s.json = append(s.json, toJSONCommit(commit))
	return nil
}

func writelog(fp io.Writer, atom io.Writer, json io.Writer, info *repoinfo, refinfo *referenceinfo) error { // int writelog(io.Writer fp, io.Writer atom, io.Writer json, const struct repoinfo* info,
	// var w *git.Revwalk = nil       // git.Revwalk* w = NULL;
	// var id git.Oid                 // git.Oid      id;
	// var ncommits int64 = 0, arsize // ssize_t      ncommits = 0, arsize;
	// var arfp io.Writer             // io.Writer        arfp;
	// var unit string                // const char*  unit;

	os.Mkdir("commit", 0777)
	os.Mkdir(refinfo.refname, 0777)

	if fp != nil { // if (fp) {
		writeheader(fp, info, 1, true, info.name, refinfo.refname) // writeheader(fp, info, 1, 1, info->name, "%s", refinfo->refname);

		fmt.Fprintf(fp, "<h2>Archives</h2>") // fmt.Fprintf("<h2>Archives</h2>", fp);
		fmt.Fprintf(fp, "<table><thead>\n<tr><td class=\"expand\">Name</td>"+
			"<td class=\"num\">Size</td></tr>\n</thead><tbody>\n")
	}

	for _, typ := range archivetypes { // FORMASK
		arfp, err := os.Create(path.Join(refinfo.refname, refinfo.refname+"."+archiveexts[typ])) // arfp   = efopen("w+", "%s/%s.%s", refinfo->refname, refinfo->refname, archiveexts[type]);
		if err != nil {
			return err
		}
		defer arfp.Close()
		arsize, err := writearchive(arfp, info, typ, refinfo) // arsize = writearchive(arfp, info, type, refinfo);
		arsize, unit := splitunit(arsize)                     // unit = splitunit(&arsize);
		if fp != nil {                                        // if (fp)
			fmt.Fprintf(fp, "<tr><td><a href=\"%s.%s\">%s.%s</a></td><td>%d%s</td></tr>",
				refinfo.refname, archiveexts[typ], refinfo.refname, archiveexts[typ], // refinfo->refname, archiveexts[type], refinfo->refname, archiveexts[type],
				arsize, unit) // arsize, unit);
		}
	}

	if fp != nil { // if (fp) {
		fmt.Fprintf(fp, "</tbody></table>") // fmt.Fprintf("</tbody></table>", fp);

		if err := writeshortlog(fp, info, refinfo.commit); err != nil { // writeshortlog(fp, info, refinfo->commit);
			return err
		}

		fmt.Fprintf(fp, "<h2>Commits of %s</h2>", refinfo.refname) // fmt.Fprintf(fp, "<h2>Commits of %s</h2>", refinfo->refname);

		fmt.Fprintf(fp, "<table id=\"log\"><thead>\n<tr><td>Date</td>"+
			"<td class=\"expand\">Commit message</td>"+
			"<td>Author</td><td class=\"num\" align=\"right\">Files</td>"+
			"<td class=\"num\" align=\"right\">+</td>"+
			"<td class=\"num\" align=\"right\">-</td></tr>\n</thead><tbody>") // "<td class=\"num\" align=\"right\">-</td></tr>\n</thead><tbody>");
	}

	/* Create a revwalk to iterate through the commits */
	w, err := info.repo.Walk()
	if err != nil {
		return fmt.Errorf("unable to initialize revwalk: %w", err)
	}

	if err := w.Push(refinfo.commit.Id()); err != nil {
		return err
	}
	ncommits := 0
	_ = w.Iterate(func(commit *git.Commit) bool {
		ncommits++
		return true
	})
	w.Reset()
	if err := w.Push(refinfo.commit.Id()); err != nil {
		return err
	}

	/* Iterate through the commits */
	var indx int = 0 // ssize_t indx = 0;
	state := logstate{fp: fp}
	_ = w.Iterate(func(commit *git.Commit) bool {
		if config.Maxcommits > 0 && indx >= config.Maxcommits {
			return false
		}
		if err = writelogcommit(&state, info, indx, commit); err != nil { // writelogcommit(fp, json, atom, info, indx, &id);
			return false
		}
		indx++ // indx++;

		printprogress(indx, ncommits, fmt.Sprintf("write log:   %-20s", refinfo.refname)) // printprogress(indx, ncommits, "write log:   %-20s", refinfo->refname);
		return true
	})
	if err != nil {
		return err
	}
	if !verbose && !quiet { // if (!verbose && !quiet)
		fmt.Println()
	}

	if fp != nil { // if (fp) {
		if config.Maxcommits > 0 && ncommits > config.Maxcommits { // if (maxcommits > 0 && ncommits > maxcommits) {
			var plural string
			if ncommits-config.Maxcommits != 1 {
				plural = "s"
			}
			fmt.Fprintf(fp, "<tr><td></td><td colspan=\"5\">%d%s commits left out...</td></tr>\n", ncommits-config.Maxcommits, plural) // ncommits - maxcommits);
		}

		fmt.Fprintf(fp, "</tbody></table>") // fmt.Fprintf("</tbody></table>", fp);
		writefooter(fp)                     // writefooter(fp);
	}
	if json != nil { // if (json)
		if err := writejsoncommits(json, state.json); err != nil {
			return err
		}
	}
	if atom != nil { // if (atom)
		if err := writeatomfeed(atom, info, state.atom); err != nil {
			return err
		}
	}
	return nil
}
