package gitin

import (
	"fmt"
	"os"
	"path"
)

// #include "common.h"
// #include "composer.h"
// #include "config.h"
// #include "writer.h"

func composerepo(info *repoinfo) error { // void composerepo(const struct repoinfo* info) {
	if !quiet { // if (!quiet) {
		if columnate { // if (columnate)
			fmt.Printf("%s\t%s\n", info.name, info.repodir) // printf("%s\t%s\n", info->name, info->repodir);
		} else {
			fmt.Printf("updating '%s' . %s\n", info.name, info.repodir) // printf("updating '%s' -> %s\n", info->name, info->repodir);
		}
	}

	for _, ref := range info.refs { // for (int i = 0; i < info->nrefs; i++) {
		os.MkdirAll(pathunhide(ref.refname), 0777) // emkdirf("%s", info->refs[i].refname);

		file, err := os.Create(path.Join(pathunhide(ref.refname), "index.html")) // fp = efopen("w", "%s/index.html", info->refs[i].refname);
		if err != nil {
			return err
		}
		defer file.Close()
		if err := writesummary(file, info, ref); err != nil { // writesummary(fp, info, &info->refs[i]);
			return err
		}

		logf, err := os.Create(path.Join(pathunhide(ref.refname), "log.html")) // fp = efopen("w", "%s/index.html", info->refs[i].refname);
		if err != nil {
			return err
		}
		defer logf.Close()
		logjson, err := os.Create(path.Join(pathunhide(ref.refname), "log.json")) // fp = efopen("w", "%s/index.html", info->refs[i].refname);
		if err != nil {
			return err
		}
		defer logjson.Close()
		logatom, err := os.Create(path.Join(pathunhide(ref.refname), "log.xml")) // fp = efopen("w", "%s/index.html", info->refs[i].refname);
		if err != nil {
			return err
		}
		defer logatom.Close()
		if err := writelog(logf, logjson, logatom, info, ref); err != nil { // writelog(fp, json, atom, info, &info->refs[i]);
			return err
		}

		if err := composefiletree(info, ref); err != nil { // composefiletree(info, &info->refs[i]);
			return err
		}
	}

	{
		file, err := os.Create("index.html") // fp = efopen("w", "%s/index.html", info->refs[i].refname);
		if err != nil {
			return err
		}
		defer file.Close()
		writeredirect(file, info.branch.refname+"/") // writeredirect(fp, "%s/", info->branch.refname);
	}
	{
		file, err := os.Create("atom.xml") // fp = efopen("w", "%s/index.html", info->refs[i].refname);
		if err != nil {
			return err
		}
		defer file.Close()
		if err := writeatomrefs(file, info); err != nil { // writeatomrefs(fp, info);
			return err
		}
	}
	{
		file, err := os.Create("index.json") // fp = efopen("w", "%s/index.html", info->refs[i].refname);
		if err != nil {
			return err
		}
		defer file.Close()
		if err := writejsonrefs(file, info); err != nil { // writejsonrefs(fp, info);
			return err
		}
	}
	return nil
}
