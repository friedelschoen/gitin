#!/usr/bin/env python3

import os
import sys
import xml.etree.ElementTree as ET
import glob

def print_error(typ, message, current, original = None, next = None):
    if original:
        sys.stderr.write(f'[{typ}] {message}: \033[1m{original}\033[0m in \033[1m{current}\033[0m\n')
    else:
        sys.stderr.write(f'[{typ}] {message}: \033[1m{current}\033[0m\n')
    if next:
        sys.stderr.write(f'      resolves to \033[1m{next}\033[0m\n')
    sys.stderr.write(f'\n')

def parse_html(file_path):
    """Parse HTML content from the given file."""
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            content = file.read()
        tree = ET.ElementTree(ET.fromstring(content))
        parents = {c: p for p in tree.iter() for c in p}
        return tree, parents
    except FileNotFoundError:
        print_error('F', 'not found', file_path)
    except ET.ParseError as err:
        print_error('F', f'parsing error ({err.msg})', file_path)
    except Exception as err:
        print_error('F', f'unknown error ({err.msg})', file_path)
    return None, None

def normalize_path(base, current_file, next_file):
    """Normalize paths for relative links."""
    if next_file.startswith('/'):
        next_file = os.path.join(base, next_file)
    else:
        next_file = os.path.join(os.path.dirname(current_file), next_file)
    
    if next_file.endswith('/'):
        next_file = os.path.join(next_file, 'index.html')

    return os.path.normpath(next_file)

def in_bounds(base, next_file):
    """Check if next_file is within the base directory."""
    # Ensure both are absolute paths
    base = os.path.abspath(base)
    next_file = os.path.abspath(next_file)
    
    # Check if next_file starts with base path
    return os.path.commonpath([base, next_file]) == base

def inside_preview(parents, element):
    """Check if the element has a <div class='preview'> ancestor."""
    while element is not None:
        if element.tag == 'div' and element.get('class') == 'preview':
            return True
        element = parents.get(element)  # Move up the tree
    return False

def find_relpath(base, current, next):
    next_file = next
    while next_file.startswith('../'):
        next_file = next_file[3:] # skip first ../
        norm = normalize_path(base, current, next_file)
        if os.path.exists(norm):
            print_error("N", "too much depth", current, next_file, norm)
            break

    next_file = next
    for _ in range(5):
        next_file = "../" + next_file
        norm = normalize_path(base, current, next_file)
        if os.path.exists(norm):
            print_error("N", "too little depth", current, next_file, norm)
            break

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <starting_file>")
        sys.exit(1)

    base = os.path.dirname(sys.argv[1])
    done = set()
    todo = {sys.argv[1]}

    fatals = 0
    errors = 0
    warns = 0

    print(f'using base: {base}')

    while todo:
        current_file = todo.pop()
        done.add(current_file)

        tree, parents = parse_html(current_file)
        if tree is None:
            fatals += 1
            continue  # Skip on parsing error or file not found
    
        # Process each link
        for link in tree.findall('.//a[@href]'):
            orig = link.attrib['href'].split('#')[0]  # Remove fragment if presen

            # Skip non-local links
            if not orig or any(s in orig for s in ('mailto:', '://', '/blob/')):
                continue

            if inside_preview(parents, link):
                print_error('I', "link inside preview", current_file, orig)
                continue

            # Normalize and check link paths
            next_file = normalize_path(base, current_file, orig)

            if next_file in done:
                continue  # Skip if already processed

            if '//' in orig:
                print_error('W', "double //", current_file, orig, next_file)
                warns += 1

            if not next_file.endswith('/') and '.' not in os.path.basename(next_file):
                print_error('W', "invalid directory-suffix", current_file, orig, next_file)
                warns += 1

            if not in_bounds(base, next_file):
                print_error('E', "out of bounds", current_file, orig, next_file)
                errors += 1
                continue

            if not os.path.exists(next_file):
                print_error('E', "not found", current_file, orig, next_file)
                errors += 1
                find_relpath(base, current_file, orig)
                continue
            
            if next_file.endswith('.html'):
                todo.add(next_file)  # Queue HTML file for processing

    # Identify orphaned files
    all_files = glob.glob('*.html', root_dir=base)
    for f in all_files:
        full_path = os.path.normpath(os.path.join(base, f))
        if full_path not in done:
            print_error('W', "orphaned", full_path)

    print(f'{len(done)} files visited, {fatals} fatal errors, {errors} errors, {warns} warnings')

if __name__ == "__main__":
    main()
