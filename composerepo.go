package gitin

import (
	"fmt"
	"os"
	"path"
)

func composerepo(info *repoinfo) error {
	if !quiet {
		if columnate {
			fmt.Printf("%s\t%s\n", info.name, info.repodir)
		} else {
			fmt.Printf("updating '%s' . %s\n", info.name, info.repodir)
		}
	}

	for _, ref := range info.refs {
		os.MkdirAll(pathunhide(ref.refname), 0777)

		file, err := os.Create(path.Join(pathunhide(ref.refname), "index.html"))
		if err != nil {
			return err
		}
		defer file.Close()
		if err := writesummary(file, info, ref); err != nil {
			return err
		}

		logf, err := os.Create(path.Join(pathunhide(ref.refname), "log.html"))
		if err != nil {
			return err
		}
		defer logf.Close()
		logjson, err := os.Create(path.Join(pathunhide(ref.refname), "log.json"))
		if err != nil {
			return err
		}
		defer logjson.Close()
		logatom, err := os.Create(path.Join(pathunhide(ref.refname), "log.xml"))
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
		writeredirect(file, info.branch.refname+"/")
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
