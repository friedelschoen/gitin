package gitin

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path"
	"strconv"
)

// #include "execute.h"

// #include "common.h"
// #include "config.h"
// #include "getinfo.h"
// #include "fmt.Fprintf.h"

// #include <sys/wait.h>
// #include <unistd.h>

type executeinfo struct {
	command     string            // const char*  command;
	fp          io.Writer         // io.Writer        fp;
	filename    string            // const char*  filename;
	contenthash uint32            // uint32_t     contenthash;
	cachename   string            // const char*  cachename;
	content     []byte            // const char*  content;
	environ     map[string]string // const char** environ;
} // };

func execute(params *executeinfo) error { // ssize_t execute(struct executeinfo* params) {
	os.MkdirAll(path.Join(".cache", params.cachename), 0777)

	if !force {
		cache, err := os.Open(path.Join(".cache", params.cachename, strconv.FormatUint(uint64(params.contenthash), 16)))
		if err == nil {
			defer cache.Close()

			_, err := cache.WriteTo(params.fp)
			return err
		}
	}

	if verbose {
		fmt.Fprintf(os.Stderr, "$ ")
		for key, value := range params.environ {
			fmt.Fprintf(os.Stderr, "%s=%s ", key, value)
		}
		fmt.Fprintf(os.Stderr, "%s\n", params.command)
	}

	cachefile, err := os.Create(path.Join(".cache", params.cachename, strconv.FormatUint(uint64(params.contenthash), 16)))
	if err != nil {
		return err
	}
	defer cachefile.Close()

	cmd := exec.Command("sh", "-c", params.command)
	cmd.Stdin = bytes.NewReader(params.content)
	cmd.Stdout = io.MultiWriter(params.fp, cachefile)
	env := os.Environ()
	for key, value := range params.environ {
		env = append(env, fmt.Sprint(key, "=", value))
	}
	cmd.Env = env
	if err := cmd.Run(); err != nil {
		return err
	}

	return nil
}
