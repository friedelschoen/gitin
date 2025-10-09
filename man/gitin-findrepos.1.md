% GITIN-FINDREPOS(1) Version %VERSION% | User Commands

# NAME
**gitin-findrepos** Recursively search for Git repositories

# SYNOPSIS
**gitin-findrepos** [*startdir*]

# DESCRIPTION
**gitin-findrepos** is a simple utility that recursively searches for Git repositories
starting from the specified directory (or the current directory if none is provided).
It outputs the paths of all found repositories.

# OPTIONS

**startdir**
The starting directory from which to begin searching.
If not specified, the current directory is used.

# EXAMPLES

**gitin-findrepos**
Search for all Git repositories starting from the current directory and output their paths.

**gitin-findrepos /home/git**
Search for all Git repositories under the `/home/git` directory and output their paths.

# SEE ALSO
**gitin(1)**
**gitin.conf(5)**

# AUTHOR
This project is founded by Hiltjo Posthuma and multiple contributors as *stagit*.
This project is forked by Friedel Schön as *gitin*.

For more information about changes and contribution, visit:

%HOMEPAGE%
