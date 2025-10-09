package main

import (
	"flag"
	"fmt"
	"os"

	"github.com/friedelschoen/gitin-go"
)

func main() {
	var pwd string

	flag.StringVar(&pwd, "C", "", "usage")
	flag.Parse()

	if pwd != "" {
		if err := os.Chdir(pwd); err != nil {
			fmt.Fprintf(os.Stderr, "error: %v\n", err)
			os.Exit(1)
		}
	}

	start := "."
	if flag.NArg() == 1 {
		start = flag.Arg(0)
	} else if flag.NArg() > 1 {
		fmt.Fprint(os.Stderr, "too many arguments")
	}

	printed := false
	for repo := range gitin.Findrepos(start) {
		fmt.Println(repo)
		printed = true
	}
	if !printed {
		fmt.Println("no repository found")
	}
}
