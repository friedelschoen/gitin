package main

import (
	"flag"
	"fmt"
	"iter"
	"os"

	"github.com/friedelschoen/gitin-go"
)

func main() {
	var pwd string
	var recursive bool
	var printVersion bool

	flag.StringVar(&pwd, "C", "", "usage")
	flag.StringVar(&gitin.Config.Configfile, "c", gitin.Config.Configfile, "configfile")
	flag.BoolVar(&recursive, "r", false, "recusive")
	flag.BoolVar(&gitin.Force, "f", false, "force")
	flag.BoolVar(&gitin.Quiet, "q", false, "quite")
	flag.BoolVar(&gitin.Columnate, "s", false, "columnate")
	flag.BoolVar(&printVersion, "V", false, "print version")
	flag.BoolVar(&gitin.Verbose, "v", false, "verbose")
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

	if file, err := os.Open(gitin.Config.Configfile); err == nil {
		defer file.Close()
		for _, value := range gitin.ParseConfig(file, gitin.Config.Configfile) {
			if err := gitin.UnmarshalConf(value, "", &gitin.Config); err != nil {
				panic(err)
			}
		}
	}

	var repos iter.Seq[string]
	if recursive {
		if flag.NArg() == 0 {
			repos = gitin.Findrepos(".")
		} else if flag.NArg() == 1 {
			repos = gitin.Findrepos(flag.Arg(0))
		} else {
			fmt.Fprint(os.Stderr, "too many arguments")
		}
	} else {
		if flag.NArg() == 0 {
			fmt.Fprint(os.Stderr, "missing argument")
		}
		repos = func(yield func(string) bool) { yield(flag.Arg(0)) }
	}

	info := gitin.GetIndex(repos)
	if err := gitin.WriteIndex(os.Stdout, info); err != nil {
		panic(err)
	}
}
