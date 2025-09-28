% GITIN(1) Version %VERSION% | User Commands

# NAME
**gitin** A Git repository browser and HTML generator

# SYNOPSIS
**gitin** [**-fhuV**] [**-C** *workdir*] [**-d** *destdir*] [**-r**] [*repo...*]

# DESCRIPTION
**gitin** is a tool to browse and generate static HTML pages from Git repositories.
It displays repository content, commit logs, file listings, and diffs as formatted HTML reports.
It allows for a customizable site generation with options for changing paths, file icons, and stylesheets.

# OPTIONS

**-f**
Force overwrite of existing files in the destination directory.

**-h**
Display help information.

**-u**
Update the generated HTML pages without regenerating the entire site.

**-C**
Specify a working directory to run the command in.
This changes to the specified directory before executing the command.

**-d**
Specify the destination directory for the generated HTML output.
Defaults to the current directory if not provided.

**-r**
Recursively search for Git repositories starting from the current or specified directory.

**-V**
Display version information.

# EXAMPLES

**gitin -C myrepo -d /var/www/html**
Generate HTML pages for the `myrepo` Git repository and output the result to `/var/www/html`.

**gitin -r -d output**
Recursively find and generate HTML reports for all Git repositories under the current directory, writing them to `output`.

# FILES
**gitin.conf**
An optional configuration file placed in the root of the repository to customize site settings such as `name`, `description`, `favicon`, and `stylesheet`.

# SEE ALSO
**gitin.conf(5)**
**gitin-findrepos(1)**

# AUTHOR
This project is founded by Hiltjo Posthuma and multiple contributors as *stagit*.
This project is forked by Friedel Schön as *gitin*.

For more information about changes and contribution, visit:

%HOMEPAGE%
