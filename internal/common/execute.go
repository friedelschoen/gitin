package common

import (
	"bytes"
	"fmt"
	"io"
	"os"
	"os/exec"
	"strings"
)

type ExecuteParams struct {
	Command   string
	Filename  string
	ID        string
	CacheName string
	Content   []byte
	Environ   map[string]string
}

func ExecuteCache(w io.Writer, params *ExecuteParams) error {
	workdir, err := os.MkdirTemp(os.TempDir(), "gitin-*")
	if err != nil {
		return fmt.Errorf("unable to create temporary directory: %w", err)
	}
	defer os.RemoveAll(workdir)

	if Verbose {
		fmt.Fprintf(os.Stderr, "$ cd %s; ", workdir)
		for key, value := range params.Environ {
			fmt.Fprintf(os.Stderr, "%s=%s ", key, value)
		}
		fmt.Fprintf(os.Stderr, " %v\n", params.Command)
	}

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
}
