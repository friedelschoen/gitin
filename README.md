# `gitin` - Static Git Page Generator

`gitin` generates static HTML pages for a git repository.

## Usage

### `gitin`

`gitin` is used to generate static pages for a git repository.

```bash
gitin [-fhsuvV] [-C workdir] [-d destdir] repos...
gitin [-fhsuvV] [-C workdir] [-d destdir] -r [startdir]
```

| Option     | Description                                                                                                    |
| ---------- | -------------------------------------------------------------------------------------------------------------- |
| -C workdir | Change to the specified working directory before operating. All other arguments must be relative to `workdir`. |
| -d destdir | Specify the destination directory. If the destination does not exist, it will be created.                      |
| -f         | Force rewriting of data, even if it is up-to-date.                                                             |
| -h         | Display the help message and exit.                                                                             |
| -s         | Columnate output, useful for scripts.                                                                          |
| -u         | Only update repositories without writing the index.                                                            |
| -V         | Print the version and exit.                                                                                    |
| -v         | Print a list of all written files.                                                                             |

### `findrepos`

`findrepos` searches for git repositories, useful for scripts or automation.

```bash
findrepos [startdir]
```

## Dependencies

- GNU Make
- C compiler (C99 standard)
- libgit2
- libarchive

## Build

To build and install:

```bash
$ make
# make install
```

## Documentation

For detailed usage, see the man pages: `gitin(1)` and `findrepos(1)`.

## Automatically Update Files on Git Push

You can automatically update the static files when pushing to a repository using a `post-receive` hook. Be aware that `git push -f` (force push) may alter the commit history, requiring a rebuild of the static pages. `gitin` only checks if the head commit has changed, so history rewrites need to be manually handled. See `gitin(1)` for more details.

Example `post-receive` hook (`repo/.git/hooks/post-receive`):

```bash
#!/bin/sh

# Detect git push -f
gitin_opt=
while read -r old new ref; do
  hasrevs=$(git rev-list "$old" "^$new" | sed 1q)
  if [ -n "$hasrevs" ]; then
    gitin_opt="$gitin_opt -f"
    break
  fi
done

gitin $gitin_opt -r .
```

## Features

- Indexes all repositories in a remote.
- Repositories can be categorized.
- Displays a log of all commits from `HEAD`.
- Shows log and diffstat for each commit.
- Displays a file tree with syntax-highlighted content.
- Shows references such as local branches and tags.
- Creates archives for each reference.
- Detects README and LICENSE files from `HEAD` and links them as web pages.
- Detects submodules (from `.gitmodules`) and links them as web pages.
- Once pages are generated (which can be relatively slow), serving them is fast, simple, and resource-efficient, requiring only a basic HTTP file server.

## Limitations

- **Not ideal for large repositories** (2000+ commits), as generating diffstats is resource-intensive.
- **Not suitable for repositories with many files**, as directory browsing is not supported yet.
- **Assumes a linear history** from `HEAD`, making it unsuitable for repositories with many branches.

  For large repositories or those with complex histories, consider using `cgit` or modifying `gitin` to run as a CGI program.

- **Initial generation can be slow**, though incremental updates are faster due to caching.
- Does not support some dynamic features of `cgit`, such as:
  - Snapshot tarballs for each commit.
  - File tree per commit.
  - Branch history logs diverging from `HEAD`.

## License

This project is licensed under the MIT License. Contributions are welcome!