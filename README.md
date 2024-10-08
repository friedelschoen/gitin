# `gitin` - Static Git Page Generator

`gitin` is a tool for generating static HTML pages to showcase a Git repository, including logs, diffs, files, and other relevant repository data.

## Features

- Indexes multiple Git repositories.
- Categorizes repositories for easy navigation.
- Displays a log of all commits from `HEAD`.
- Shows commit logs and diffstat for each commit.
- Generates a filetree with syntax-highlighted file content.
- Optionally generates a per-directory filetree rather than one big merged filetree.
- Shows local branches and tags.
- Creates downloadable tarballs for each reference.
- Automatically detects and displays important files like `README` and `LICENSE` from `HEAD`.
- Detects and links submodules from `.gitmodules`.
- Provides Atom feeds for commit and tag history.
- Generates JSON data with all commits and references.
- Fast and simple to serve after pages are generated (via any HTTP server).

## Limitations

- Not ideal for large histories (2000+ commits) due to performance limitations with diffstat generation.
- Assumes a linear history from `HEAD`, which may not work well with complex repositories featuring many branches.
- Initial generation can be slow, but subsequent updates are faster due to caching.
- Does not support advanced dynamic features like:
  - Snapshot tarballs for each commit.
  - File tree per commit.
  - Branch history logs diverging from `HEAD`.

  For dynamic features consider using tools like `cgit` or `gitweb`.

## Usage

### `gitin`

`gitin` generates static pages for one or more Git repositories. It can also index multiple repositories.

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

### `gitin-findrepos`

`gitin-findrepos` is a utility that searches for Git repositories within a specified directory. This is useful for automating the generation of static pages for multiple repositories.

```bash
gitin-findrepos [startdir]
```

## Dependencies

- [GNU Make](https://www.gnu.org/software/make/) - Build system.
- [C compiler](https://gcc.gnu.org/) - For compiling code.
- [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/) - To get library requirements.
- [libgit2](https://libgit2.org/) - Git support library.
- [libarchive](https://libarchive.org/) - For handling archive creation.
- [chroma](https://github.com/alecthomas/chroma) - Syntax highlighter for source code.

## Build

To build and install `gitin`:

```
$ make
# make install
```

## Documentation

For more detailed usage instructions, see the man pages: `gitin(1)`, `gitin.conf(5)` and `gitin-findrepos(1)`.

## License

This project is licensed under the MIT License. Contributions are welcome!
