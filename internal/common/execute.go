package common

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path"
	"strconv"
	"strings"
)

type ExecuteParams struct {
	Command   string
	Filename  string
	Hash      uint32
	CacheName string
	Content   []byte
	Environ   map[string]string
}

func ExecuteCache(w io.Writer, params *ExecuteParams) error {
	cachepath := path.Join(".cache", params.CacheName, strconv.FormatUint(uint64(params.Hash), 16))
	return CachedWriter(w, cachepath, func(w io.Writer) error {
		if Verbose {
			fmt.Fprintf(os.Stderr, "$ ")
			for key, value := range params.Environ {
				fmt.Fprintf(os.Stderr, "%s=%s ", key, value)
			}
			fmt.Fprintf(os.Stderr, "%s\n", params.Command)
		}

		workdir, err := os.MkdirTemp(os.TempDir(), "gitin-*")
		if err != nil {
			return fmt.Errorf("unable to create temporary directory: %w", err)
		}
		defer os.RemoveAll(workdir)

		cmd := exec.Command("sh", "-c", params.Command)
		cmd.Dir = workdir
		cmd.Stdin = bytes.NewReader(params.Content)
		cmd.Stdout = w
		var stderr bytes.Buffer
		cmd.Stderr = &stderr
		env := os.Environ()
		for key, value := range params.Environ {
			env = append(env, fmt.Sprint(key, "=", value))
		}
		cmd.Env = env
		if err := cmd.Run(); err != nil {
			var envstr strings.Builder
			for key, value := range params.Environ {
				fmt.Fprintf(&envstr, "%s=%s ", key, value)
			}
			return fmt.Errorf("command '%s%s' failed: %w\n%s", &envstr, params.Command, err, &stderr)
		}
		return nil
	})
}
