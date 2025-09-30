#!/usr/bin/env python3

import io
import sys
import traceback
import yaml

try:
    import tomllib  # For Python >= 3.11
except ImportError:
    import toml     # For Python <= 3.10

def parseini(f):
    config = {}
    current_section = None

    for line in f:
        if line.startswith(';'):
            line = line[:line.index(';')]

        if line.startswith('#'):
            line = line[:line.index('#')]

        line = line.strip()

        # Skip comments and empty lines
        if not line:
            continue

        # Section header
        if line.startswith('[') and line.endswith(']'):
            current_section = line[1:-1].strip()
            config[current_section] = {}
        elif '=' in line and current_section is not None:
            # Key-value pair
            key, value = map(str.strip, line.split('=', 1))
            config[current_section][key] = value
        else:
            raise ValueError(f"Line '{line}' is not valid INI format.")

    return config

def writehtml(data, indent_level=0):
    indent = "    " * indent_level

    if isinstance(data, dict):
        # Use <ul> for dictionaries
        sys.stdout.write(f"{indent}<ul>\n")
        for key, value in data.items():
            sys.stdout.write(f"{indent}    <li><b>{key}:</b>\n")
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

def main():
    if len(sys.argv) != 2:
        sys.stderr.write("Usage: python gitin-configtree.py <yaml|ini|toml>\n")
        sys.exit(1)

    # Read input from stdin
    file_type = sys.argv[1].lower()

    # Process the content based on file type and directly write to stdout
    try:
        if file_type == "yaml":
            writehtml(yaml.safe_load(sys.stdin))
        elif file_type == "ini":
            writehtml(parseini(sys.stdin))
        elif file_type == "toml":
            bstdin = io.BufferedReader(sys.stdin.buffer)
            writehtml(tomllib.load(bstdin))
        else:
            sys.stdout.write("<p>Unsupported file format</p>\n")

    except Exception as err:
        traceback.format_exc()
        sys.stderr.write(f'{err!r}\n')
        sys.stdout.write("<p><i>Unable to parse config</i></p>\n")

if __name__ == '__main__':
    main()