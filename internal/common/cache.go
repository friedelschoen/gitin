package common

import (
	"io"
	"os"
	"path"
)

// CachedWriter wraps a writer-function. If the file at `cachepath` exist and is readable, it will read the contents to `w`. If not, it will call `stream` which directly writes to `w` and makes a `cache`.
func CachedWriter(w io.Writer, cachepath string, stream func(io.Writer) error) error {
	if !Force {
		cache, err := os.Open(cachepath)
		if err == nil {
			defer cache.Close()

			_, err := cache.WriteTo(w)
			return err
		}
	}

	os.MkdirAll(path.Dir(cachepath), 0777)
	cache, err := os.Create(cachepath)
	if err != nil {
		return err
	}
	defer cache.Close()

	return stream(io.MultiWriter(w, cache))
}
