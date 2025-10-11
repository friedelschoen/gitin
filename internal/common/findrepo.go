package common

import (
	"io/fs"
	"iter"
	"os"
	"path"
	"path/filepath"
)

func checkrepo(base, pathname string) bool {
	if stat, err := os.Stat(path.Join(base, pathname, "HEAD")); err == nil && stat.Mode().IsRegular() {
		return true
	}
	if stat, err := os.Stat(path.Join(base, pathname, ".git", "HEAD")); err == nil && stat.Mode().IsRegular() {
		return true
	}
	return false
}

func Findrepos(base_path string) iter.Seq[string] {
	return func(yield func(string) bool) {
		_ = filepath.WalkDir(base_path, func(entry string, d fs.DirEntry, err error) error {
			if !d.IsDir() {
				return nil
			}
			if checkrepo(base_path, entry) {
				if !yield(path.Join(base_path, entry)) {
					return filepath.SkipAll
				}
			}
			return nil
		})
	}
}
