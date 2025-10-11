package main

import (
	"flag"
	"fmt"
	"log"
	"os"

	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
	"github.com/friedelschoen/gitin-go/internal/writer"
)

func main() {
	var pwd string
	var relpath = -1
	var printVersion bool

	flag.StringVar(&pwd, "C", "", "usage")
	flag.StringVar(&gitin.Config.Configfile, "c", gitin.Config.Configfile, "configfile")
	flag.IntVar(&relpath, "d", 0, "relpath")
	flag.BoolVar(&common.Force, "f", false, "force")
	flag.BoolVar(&common.Quiet, "q", false, "quite")
	flag.BoolVar(&common.Columnate, "s", false, "columnate")
	flag.BoolVar(&printVersion, "V", false, "print version")
	flag.BoolVar(&common.Verbose, "v", false, "verbose")
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

	if file, err := os.Open(gitin.Config.Configfile); err == nil {
		defer file.Close()
		for _, value := range common.ParseConfig(file, gitin.Config.Configfile) {
			if err := common.UnmarshalConf(value, "", &gitin.Config); err != nil {
				panic(err)
			}
		}
	}

	repoinfo, err := wrapper.Getrepo(repodir, relpath)
	if err != nil {
		panic(err)
	}
	err = writer.Composerepo(repoinfo)
	if err != nil {
		log.Fatalln(err)
	}
}
