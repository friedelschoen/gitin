% GITIN-CONFIGTREE(1) Version %VERSION% | User Commands

# NAME
**gitin-configtree** parse and convert YAML, INI, or TOML configuration files to HTML format

# SYNOPSIS
**gitin-configtree** <yaml|ini|toml>

# DESCRIPTION
**gitin-configtree** is a command-line tool that reads configuration files in YAML, INI, or TOML format from standard input, parses the content, and outputs the data as HTML.
The output HTML structure reflects the nested nature of the original configuration file, with lists and dictionaries represented by HTML lists.

This tool can be useful for displaying configuration information in a web-friendly format and is capable of handling typical features of each configuration format.

# OPTIONS

**yaml**
Interpret the input as a YAML file. YAML data will be parsed and output as HTML.
YAML files can include complex nesting, lists, and dictionaries.

**ini**
Interpret the input as an INI file. INI sections will be parsed and output as HTML.
This mode supports basic key-value pairs and section headers but is limited in nesting due to INI format constraints.

**toml**
Interpret the input as a TOML file. TOML data will be parsed and output as HTML.
TOML files can include nested tables and lists.

# USAGE
`gitin-configtree` expects a single argument specifying the format of the input file and reads the file from standard input.

# EXIT STATUS

**0**
The program successfully parsed and output HTML.

**1**
An error occurred, such as an unsupported file format or a parsing error.

# SEE ALSO
**gitin(1)**

# AUTHOR
This project is founded by Hiltjo Posthuma and multiple contributors as *stagit*.
This project is forked by Friedel Schön as *gitin*.

For more information about changes and contribution, visit:

%HOMEPAGE%
