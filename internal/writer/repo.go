// Package writer describes writes used to build a repository-tree
package writer

import (
	"errors"
	"fmt"
	"io"
	"os"
	"path"
	"strings"
	"sync"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/render"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
)

func makelog(fp io.Writer, svg io.Writer, atom io.Writer, json io.Writer, info *wrapper.RepoInfo, refinfo *wrapper.ReferenceInfo) error {
	os.Mkdir("commit", 0777)
	os.Mkdir(refinfo.Refname, 0777)

	if fp != nil {
		render.WriteHeader(fp, info, 1, true, info.Name, refinfo.Refname)

		fmt.Fprintf(fp, "<h2>Archives</h2>")
		fmt.Fprintf(fp, "<table><thead>\n<tr><td class=\"expand\">Name</td>"+
			"<td class=\"num\">Size</td></tr>\n</thead><tbody>\n")
	}

	for _, ext := range gitin.Config.Archives {
		ext = strings.TrimPrefix(ext, ".") /* .tar.xz -> tar.xz, .zip -> zip */
		arfp, err := os.Create(path.Join(refinfo.Refname, refinfo.Refname+"."+ext))
		if err != nil {
			return err
		}
		defer arfp.Close()
		arsize, err := render.WriteArchive(arfp, info, ext, refinfo)
		if err != nil {
			return err
		}
		arsize, unit := common.Splitunit(arsize)
		if fp != nil {
			fmt.Fprintf(fp, "<tr><td><a href=\"%s.%s\">%s.%s</a></td><td>%d%s</td></tr>", refinfo.Refname, ext, refinfo.Refname, ext, arsize, unit)
		}
	}

	if fp != nil {
		fmt.Fprintf(fp, "</tbody></table>")
	}

	err := render.WriteLog(fp, svg, json, atom, info, refinfo)

	if fp != nil {
		render.WriteFooter(fp)
	}

	return err
}

func MakeRepo(info *wrapper.RepoInfo) error {
	if !common.Quiet {
		if common.Columnate {
			fmt.Printf("%s\t%s\n", info.Name, info.Repodir)
		} else {
			fmt.Printf("updating '%s' . %s\n", info.Name, info.Repodir)
		}
	}

	var (
		wg   sync.WaitGroup
		errs []error
	)
	for _, ref := range info.Refs {
		os.MkdirAll(common.Pathunhide(ref.Refname), 0777)

		wg.Add(3)
		go func() {
			defer wg.Done()
			file, err := os.Create(path.Join(common.Pathunhide(ref.Refname), "index.html"))
			if err != nil {
				errs = append(errs, err)
				return
			}
			defer file.Close()
			if err := render.WriteSummary(file, info, ref); err != nil {
				errs = append(errs, err)
				return
			}
		}()

		go func() {
			defer wg.Done()
			logf, err := os.Create(path.Join(common.Pathunhide(ref.Refname), "log.html"))
			if err != nil {
				errs = append(errs, err)
				return
			}
			logsvg, err := os.Create(path.Join(common.Pathunhide(ref.Refname), "log.svg"))
			if err != nil {
				errs = append(errs, err)
				return
			}
			defer logf.Close()
			logjson, err := os.Create(path.Join(common.Pathunhide(ref.Refname), "log.json"))
			if err != nil {
				errs = append(errs, err)
				return
			}
			defer logjson.Close()
			logatom, err := os.Create(path.Join(common.Pathunhide(ref.Refname), "log.xml"))
			if err != nil {
				errs = append(errs, err)
				return
			}
			defer logatom.Close()
			if err := makelog(logf, logsvg, logjson, logatom, info, ref); err != nil {
				errs = append(errs, err)
				return
			}
		}()

		go func() {
			defer wg.Done()
			if err := makefiletree(info, ref); err != nil {
				errs = append(errs, err)
				return
			}
		}()
	}
	wg.Wait()
	if len(errs) > 0 {
		return errors.Join(errs...)
	}

	{
		file, err := os.Create("index.html")
		if err != nil {
			return err
		}
		defer file.Close()
		render.WriteRedirect(file, info.Branch.Refname+"/")
	}
	{
		file, err := os.Create("atom.xml")
		if err != nil {
			return err
		}
		defer file.Close()
		if err := render.WriteAtomRefs(file, info); err != nil {
			return err
		}
	}
	{
		file, err := os.Create("index.json")
		if err != nil {
			return err
		}
		defer file.Close()
		if err := render.WriteJSONRefs(file, info); err != nil {
			return err
		}
	}
	return nil
}
