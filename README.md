# `gitin` - Static Git Page Generator

`gitin` is a tool for generating static HTML pages to showcase a Git repository, including logs, diffs, files, and other relevant repository data.

## Features

- **Generates a File Tree with Syntax-Highlighted File Content**: This feature makes the static HTML pages visually appealing and functional for code review, supporting multiple languages and handling larger files efficiently.
  
- **Automatically Detects and Displays Important Files like `README` and `LICENSE` from `HEAD**: These files provide critical information to users and are automatically displayed for better usability.
  
- **Indexes Multiple Git Repositories**: Supports indexing multiple repositories, making it valuable for managing a large set of projects. This can be expanded with per-repository configuration options.
  
- **Generates JSON Data with All Commits and References**: Allows integration with other systems or custom visualizations by providing structured data.
  
- **Optionally Generates a Per-Directory File Tree Rather than One Big Merged File Tree**: Offers flexibility for large repositories where a single file tree might become too large or complex.
  
- **Shows Commit Logs and Diffstat for Each Commit**: Displays commit logs and detailed diffs to help users understand the changes in the repository.
  
- **Provides Atom Feeds for Commit and Tag History**: Atom feeds enable users to track repository updates in their feed readers.
  
- **Creates Downloadable Tarballs for Each Reference**: Useful for users who want to download specific versions of the repository without using Git.
  
- **Categorizes Repositories for Easy Navigation**: Helps organize repositories, especially in larger projects with multiple repositories.
  
- **Fast and Simple to Serve After Pages are Generated**: Once the pages are generated, they can be served quickly and efficiently using any HTTP server.
  
- **Preview Markdown, reStructuredText, and CSV with `pandoc`**: Built-in previews for common documentation formats like Markdown, reStructuredText, and CSV improve the usability of the generated HTML.
  
- **Preview Images**: Automatically generates image previews for repositories containing visual content like design files or diagrams.
  
- **Shows Local Branches and Tags**: Displays information about local branches and tags to give users an overview of the repository’s structure.

- **Shows a Commits by Author Table**: `git shortlog -s` show the amount of commits made by a author, this is shown in a table

- **Shows Commit Timeline**: Generates a graph with how many commits are made in which months.


## Limitations

- **Not Ideal for Large Histories (2000+ Commits)**: Generating diffstat for a large number of commits can be resource-intensive and slow.
  
- **Assumes a Linear History from `HEAD`**: Works best with repositories that have a simple linear history; complex repositories with many branches may not be well-suited.
  
- **Initial Generation Can Be Slow**: The first time the pages are generated, it may take some time, though subsequent updates are faster due to caching.

- **Does Not Support Advanced Dynamic Features**: Lacks features like snapshot tarballs for each commit, file trees for each commit, or branch history logs that diverge from `HEAD`. Consider using dynamic tools like `cgit` or `gitweb` for those features.

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

- [GNU Make](https://gnu.org/software/make/) - Build system.
- [C compiler](https://gcc.gnu.org/) - For compiling code.
- [pkg-config](https://freedesktop.org/wiki/Software/pkg-config/) - To get library requirements.
- [libgit2](https://libgit2.org/) - Git support library.
- [libarchive](https://libarchive.org/) - For handling archive creation.
- [chroma](https://github.com/alecthomas/chroma) - Syntax highlighter for source code.
- [pandoc](https://pandoc.org/) - Preview Markdown, RST and other formats
- [python3](https://python.org/) - Interpreter for gitin-configtree
- [pyyaml](https://pyyaml.org/) - YAML-parser for gitin-configtree

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
