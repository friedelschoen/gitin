package writer

import (
	"fmt"
	"os"
	"path"

	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
)

func Composerepo(info *wrapper.RepoInfo) error {
	if !common.Quiet {
		if common.Columnate {
			fmt.Printf("%s\t%s\n", info.Name, info.Repodir)
		} else {
			fmt.Printf("updating '%s' . %s\n", info.Name, info.Repodir)
		}
	}

	for _, ref := range info.Refs {
		os.MkdirAll(common.Pathunhide(ref.Refname), 0777)

		file, err := os.Create(path.Join(common.Pathunhide(ref.Refname), "index.html"))
		if err != nil {
			return err
		}
		defer file.Close()
		if err := writesummary(file, info, ref); err != nil {
			return err
		}

		logf, err := os.Create(path.Join(common.Pathunhide(ref.Refname), "log.html"))
		if err != nil {
			return err
		}
		defer logf.Close()
		logjson, err := os.Create(path.Join(common.Pathunhide(ref.Refname), "log.json"))
		if err != nil {
			return err
		}
		defer logjson.Close()
		logatom, err := os.Create(path.Join(common.Pathunhide(ref.Refname), "log.xml"))
		if err != nil {
			return err
		}
		defer logatom.Close()
		if err := writelog(logf, logjson, logatom, info, ref); err != nil {
			return err
		}

		if err := composefiletree(info, ref); err != nil {
			return err
		}
	}

	{
		file, err := os.Create("index.html")
		if err != nil {
			return err
		}
		defer file.Close()
		writeredirect(file, info.Branch.Refname+"/")
	}
	{
		file, err := os.Create("atom.xml")
		if err != nil {
			return err
		}
		defer file.Close()
		if err := writeatomrefs(file, info); err != nil {
			return err
		}
	}
	{
		file, err := os.Create("index.json")
		if err != nil {
			return err
		}
		defer file.Close()
		if err := writejsonrefs(file, info); err != nil {
			return err
		}
	}
	return nil
}
