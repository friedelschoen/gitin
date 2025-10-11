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
	Command     string
	Destination io.Writer
	Filename    string
	Hash        uint32
	CacheName   string
	Content     []byte
	Environ     map[string]string
}

func ExecuteCache(params *ExecuteParams) error {
	os.MkdirAll(path.Join(".cache", params.CacheName), 0777)

	if !Force {
		cache, err := os.Open(path.Join(".cache", params.CacheName, strconv.FormatUint(uint64(params.Hash), 16)))
		if err == nil {
			defer cache.Close()

			_, err := cache.WriteTo(params.Destination)
			return err
		}
	}

	if Verbose {
		fmt.Fprintf(os.Stderr, "$ ")
		for key, value := range params.Environ {
			fmt.Fprintf(os.Stderr, "%s=%s ", key, value)
		}
		fmt.Fprintf(os.Stderr, "%s\n", params.Command)
	}

	tempdir, err := os.MkdirTemp(os.TempDir(), "gitin-*")
	if err != nil {
		return fmt.Errorf("unable to create temporary directory: %w", err)
	}
	defer os.RemoveAll(tempdir)

	cachefile, err := os.Create(path.Join(".cache", params.CacheName, strconv.FormatUint(uint64(params.Hash), 16)))
	if err != nil {
		return err
	}
	defer cachefile.Close()

	cmd := exec.Command("sh", "-c", params.Command)
	cmd.Dir = tempdir
	cmd.Stdin = bytes.NewReader(params.Content)
	cmd.Stdout = io.MultiWriter(params.Destination, cachefile)
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
