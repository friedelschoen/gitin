#!/usr/bin/env python3

import yaml
import configparser
import sys

try:
    import tomllib  # For Python >= 3.11
except ImportError:
    import toml     # For Python <= 3.10

def writehtml(data, indent_level=0):
    indent = "    " * indent_level

    if isinstance(data, dict):
        # Use <ul> for dictionaries
        sys.stdout.write(f"{indent}<ul>\n")
        for key, value in data.items():
            sys.stdout.write(f"{indent}    <li><strong>{key}:</strong>\n")
            writehtml(value, indent_level + 2)
            sys.stdout.write(f"{indent}    </li>\n")
        sys.stdout.write(f"{indent}</ul>\n")
    elif isinstance(data, list):
        # Use <ol> for lists
        sys.stdout.write(f"{indent}<ol>\n")
        for item in data:
            sys.stdout.write(f"{indent}    <li>\n")
            writehtml(item, indent_level + 2)
            sys.stdout.write(f"{indent}    </li>\n")
        sys.stdout.write(f"{indent}</ol>\n")
    else:
        sys.stdout.write(f"{indent}{data}\n")

if len(sys.argv) != 2:
    sys.stderr.write("Usage: python gitin-configtree <file_type>\n")
    sys.stderr.write("<file_type> should be one of: yaml, ini, toml\n")
    sys.exit(1)

# Read input from stdin
file_type = sys.argv[1].lower()
content = sys.stdin.read()

# Process the content based on file type and directly write to stdout
if file_type == "yaml":
    writehtml(yaml.safe_load(content))
elif file_type == "ini":
    writehtml(parseini(content))
elif file_type == "toml":
    writehtml(tomllib.loads(toml_content))
else:
    sys.stdout.write("<p>Unsupported file format</p>\n")