package main

import (
	"flag"
	"fmt"
	"iter"
	"os"

	"github.com/BurntSushi/toml"
	"github.com/friedelschoen/gitin-go"
	"github.com/friedelschoen/gitin-go/internal/common"
	"github.com/friedelschoen/gitin-go/internal/render"
	"github.com/friedelschoen/gitin-go/internal/wrapper"
)

func main() {
	var pwd string
	var recursive bool
	var printVersion bool

	flag.StringVar(&pwd, "C", "", "usage")
	flag.StringVar(&gitin.Configfile, "c", gitin.Configfile, "configfile")
	flag.BoolVar(&recursive, "r", false, "recusive")
	flag.BoolVar(&common.Force, "f", false, "force")
	flag.BoolVar(&common.Quiet, "q", false, "quite")
	flag.BoolVar(&common.Columnate, "s", false, "columnate")
	flag.BoolVar(&printVersion, "V", false, "print version")
	flag.BoolVar(&common.Verbose, "v", false, "verbose")
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

	if file, err := os.Open(gitin.Configfile); err == nil {
		defer file.Close()
		if _, err := toml.NewDecoder(file).Decode(&gitin.Config); err != nil {
			panic(err)
		}
	}

	var repos iter.Seq[string]
	if recursive {
		if flag.NArg() == 0 {
			repos = common.Findrepos(".")
		} else if flag.NArg() == 1 {
			repos = common.Findrepos(flag.Arg(0))
		} else {
			fmt.Fprint(os.Stderr, "too many arguments")
		}
	} else {
		if flag.NArg() == 0 {
			fmt.Fprint(os.Stderr, "missing argument")
		}
		repos = func(yield func(string) bool) { yield(flag.Arg(0)) }
	}

	info := wrapper.GetIndex(repos)
	if err := render.WriteIndex(os.Stdout, info); err != nil {
		panic(err)
	}
}
