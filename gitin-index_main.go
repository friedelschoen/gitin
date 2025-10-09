package gitin

import (
	"flag"
	"fmt"
	"iter"
	"os"
)

func GitinIndexMain() {
	var pwd string
	var recursive bool
	var printVersion bool

	flag.StringVar(&pwd, "C", "", "usage")
	flag.StringVar(&config.Configfile, "c", config.Configfile, "configfile")
	flag.BoolVar(&recursive, "r", false, "recusive")
	flag.BoolVar(&force, "f", false, "force")
	flag.BoolVar(&quiet, "q", false, "quite")
	flag.BoolVar(&columnate, "s", false, "columnate")
	flag.BoolVar(&printVersion, "V", false, "print version")
	flag.BoolVar(&verbose, "v", false, "verbose")
	flag.Parse()

	if printVersion {
		fmt.Println("v0.1.0")
	}

	if pwd != "" {
		if err := os.Chdir(pwd); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
	}

	if file, err := os.Open(config.Configfile); err == nil { // if ((fp = efopen("!.r", "%s/%s", info->repodir, configfile))) {
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

	var repos iter.Seq[string]
	if recursive {
		if flag.NArg() == 0 {
			repos = findrepos(".")
		} else if flag.NArg() == 1 {
			repos = findrepos(flag.Arg(0))
		} else {
			fmt.Fprint(os.Stderr, "too many arguments")
		}
	} else {
		if flag.NArg() == 0 {
			fmt.Fprint(os.Stderr, "missing argument")
		}
		repos = func(yield func(string) bool) { yield(flag.Arg(0)) }
	}

	info := getindex(repos)
	if err := writeindex(os.Stdout, info); err != nil {
		panic(err)
	}
}
