#!/usr/bin/env python3

import os
import sys
import xml.etree.ElementTree as ET
import glob

base = os.path.dirname(sys.argv[1])
done = set()
todo = { sys.argv[1] }

while todo:
	current_file = todo.pop()
	done.add(current_file)

	# Read the HTML content from the file
	try:
		with open(current_file, 'r', encoding='utf-8') as file:
			content = file.read()

		tree = ET.ElementTree(ET.fromstring(content))
	except FileNotFoundError:
		print(f"file not found: {current_file}")
		continue
	except ET.ParseError:
		print(f"error parsing file: {current_file}")
		continue
	except Exception as err:
		print(f"unknown error {err!s}: {current_file}")

	# Find all 'a' tags with 'href' attributes
	links = tree.findall('.//a[@href]')
	
	# Check each link's status
	for link in links:
		next_file = orig = link.attrib['href']

		if '#' in next_file:
			next_file = next_file[:next_file.index('#')]

		if not next_file:
			continue

		if next_file.startswith('mailto:'):
			continue

		if '://' in next_file:
			continue

		if '/blob/' in next_file:
			continue

		if '//' in next_file:
			print(f"// {current_file} -> {orig} ({next_file})")

		if next_file.startswith('/'):
			next_file = base + "/" + next_file
		else:
			next_file = os.path.dirname(current_file) + "/" + next_file

		if next_file.endswith('/'):
			next_file = os.path.join(next_file, 'index.html')

		next_file = os.path.normpath(next_file)
		
		if next_file in done:
			continue

		if not os.path.exists(next_file):
			print(f"{current_file} -> {orig} ({next_file})")
			continue
		
		if next_file.endswith('.html'):
			# Add the new file to the todo set if not already done
			todo.add(next_file)

all_files = glob.glob('*.html', root_dir=base)
for f in all_files:
	if f in done: continue

	print(f"orphaned: {f}")