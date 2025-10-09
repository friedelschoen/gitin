package gitin

import (
	"flag"
	"fmt"
	"os"
)

func GitinMain() {
	var pwd string
	var relpath = -1
	var printVersion bool

	flag.StringVar(&pwd, "C", "", "usage")
	flag.StringVar(&config.Configfile, "c", config.Configfile, "configfile")
	flag.IntVar(&relpath, "d", 0, "relpath")
	flag.BoolVar(&force, "f", false, "force")
	flag.BoolVar(&quiet, "q", false, "quite")
	flag.BoolVar(&columnate, "s", false, "columnate")
	flag.BoolVar(&printVersion, "V", false, "print version")
	flag.BoolVar(&verbose, "v", false, "verbose")
	flag.Parse()

	if printVersion {
		fmt.Println("v0.1.0")
		os.Exit(1)
	}

	repodir := "."

	if flag.NArg() > 0 {
		repodir = flag.Arg(0)
	}
	if pwd != "" {
		if err := os.Chdir(pwd); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
	}

	if file, err := os.Open(config.Configfile); err == nil {
		defer file.Close()
		for _, value := range ParseConfig(file, config.Configfile) {
			if err := UnmarshalConf(value, "", &config); err != nil {
				panic(err)
			}
		}
	}

	if config.Archivetargz {
		archivetypes = append(archivetypes, ArchiveTarGz)
	}
	if config.Archivetarxz {
		archivetypes = append(archivetypes, ArchiveTarXz)
	}
	if config.Archivetarbz2 {
		archivetypes = append(archivetypes, ArchiveTarBz2)
	}
	if config.Archivezip {
		archivetypes = append(archivetypes, ArchiveZip)
	}

	repoinfo, err := getrepo(repodir, relpath)
	if err != nil {
		panic(err)
	}
	err = composerepo(repoinfo)
	if err != nil {
		panic(err)
	}

}
