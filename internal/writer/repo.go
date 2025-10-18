// Package writer describes writes used to build a repository-tree
package writer

import (
	"errors"
	"fmt"
	"os"
	"path"
	"strings"
	"sync"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/render"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
)

func createAndPrint(name string) (*os.File, error) {
	if common.Verbose {
		fmt.Println(name)
	}
	return os.Create(name)
}

func makeRefs(info *wrapper.RepoInfo) error {
	commitCache := make(map[string]*wrapper.CommitInfo)
	commitsWritten := make(map[string]bool)
	for _, ref := range info.Refs {
		var (
			commits  []*wrapper.CommitInfo
			ncommits int
			stats    *render.LogStats
			arsizes  = make(map[string]int64)
			wg       sync.WaitGroup
			errs     []error
		)

		wg.Add(1)
		go func() {
			defer wg.Done()

			var err error
			commits, ncommits, err = wrapper.GetCommits(info.Repo, ref, commitCache)
			if err != nil {
				errs = append(errs, err)
			}
		}()

		wg.Add(1)
		go func() {
			defer wg.Done()
			for _, ext := range gitin.Config.Archives {
				ext = strings.TrimPrefix(ext, ".") /* .tar.xz -> tar.xz, .zip -> zip */
				arfp, err := createAndPrint(path.Join(ref.Refname, ref.Refname+"."+ext))
				if err != nil {
					errs = append(errs, err)
					return
				}
				defer arfp.Close()
				arsize, err := render.WriteArchive(arfp, info.Repo, ext, ref)
				if err != nil {
					errs = append(errs, err)
					return
				}
				arsizes[ext] = arsize
			}
		}()

		wg.Add(1)
		go func() {
			defer wg.Done()

			var err error
			stats, err = render.GetLogStats(info.Repo, ref.Commit)
			if err != nil {
				errs = append(errs, err)
				return
			}
		}()

		wg.Wait()

		if err := errors.Join(errs...); err != nil {
			return err
		}

		{
			file, err := createAndPrint(path.Join(common.Pathunhide(ref.Refname), "index.html"))
			if err != nil {
				return err
			}
			defer file.Close()
			if err := render.WriteSummary(file, info, ref); err != nil {
				return err
			}
		}
		{
			fp, err := createAndPrint(path.Join(common.Pathunhide(ref.Refname), "log.html"))
			if err != nil {
				return err
			}
			defer fp.Close()
			if err := render.WriteLog(fp, info, ref.Refname, commits, ncommits, stats, arsizes); err != nil {
				return err
			}
		}

		for _, commit := range commits {
			if iswritten, ok := commitsWritten[commit.ID]; ok && iswritten {
				continue
			}
			pat := path.Join("commit", commit.ID+".html")

			commitfile, err := createAndPrint(pat)
			if err != nil {
				return err
			}
			defer commitfile.Close()
			if err := render.WriteCommit(commitfile, info, commit); err != nil {
				return err
			}
			commitsWritten[commit.ID] = true
		}

		{
			logsvg, err := createAndPrint(path.Join(common.Pathunhide(ref.Refname), "log.svg"))
			if err != nil {
				return err
			}
			defer logsvg.Close()
			stats.WriteDiagram(logsvg)
		}
		{
			logjson, err := createAndPrint(path.Join(common.Pathunhide(ref.Refname), "log.json"))
			if err != nil {
				return err
			}
			defer logjson.Close()
			if err := render.WriteLogJSON(logjson, ref.Refname, commits, ncommits); err != nil {
				return err
			}
		}
		{
			logatom, err := createAndPrint(path.Join(common.Pathunhide(ref.Refname), "log.xml"))
			if err != nil {
				return err
			}
			defer logatom.Close()
			if err := render.WriteLogAtom(logatom, info, ref); err != nil {
				return err
			}
		}
	}
	return nil
}

func MakeRepo(info *wrapper.RepoInfo) error {
	if !common.Quiet {
		if common.Columnate {
			fmt.Printf("%s\t%s\n", info.Name, info.Repodir)
		} else {
			fmt.Printf("updating '%s' . %s\n", info.Name, info.Repodir)
		}
	}

	for _, ref := range info.Refs {
		os.MkdirAll(common.Pathunhide(ref.Refname), 0777)
		os.MkdirAll("commit", 0777)
		os.MkdirAll(ref.Refname, 0777)
	}

	var (
		wg   sync.WaitGroup
		errs []error
	)

	wg.Add(1)
	go func() {
		defer wg.Done()
		if err := makeRefs(info); err != nil {
			errs = append(errs, err)
			return
		}
	}()

	for _, ref := range info.Refs {
		wg.Add(1)
		go func() {
			defer wg.Done()
			if err := makefiletree(info, ref); err != nil {
				errs = append(errs, err)
				return
			}
		}()
	}

	wg.Wait()

	{
		file, err := createAndPrint("index.html")
		if err != nil {
			return err
		}
		defer file.Close()
		render.WriteRedirect(file, info.Branch.Refname+"/")
	}
	{
		file, err := createAndPrint("atom.xml")
		if err != nil {
			return err
		}
		defer file.Close()
		if err := render.WriteAtomRefs(file, info); err != nil {
			return err
		}
	}
	{
		file, err := createAndPrint("index.json")
		if err != nil {
			return err
		}
		defer file.Close()
		if err := render.WriteJSONRefs(file, info.Refs); err != nil {
			return err
		}
	}
	return nil
}
