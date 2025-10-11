package main

import (
	"fmt"
	"os"

	"github.com/friedelschoen/gitin-go/internal/preview"
)

func main() {
	if len(os.Args) != 2 {
		fmt.Fprintln(os.Stderr, "Usage: gitin-configtree <json|yaml|yml|ini|toml|plist>")
		os.Exit(1)
	}

	err := preview.WriteConfigTree(os.Stdout, os.Stdin, os.Args[1])
	if err != nil {
		fmt.Fprintln(os.Stderr, "error: ", err)
	}
}
