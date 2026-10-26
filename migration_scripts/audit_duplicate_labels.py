"""Provenance-free duplicate-label scan.

Reports every label defined more than once within a single file, and every label
defined in more than one file, across the files data/event_scripts.s pulls in.
Labels that repeat inside one file across assembler .if/.else branches are listed
separately because they are deliberate variants.
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

INC_RE = re.compile(r'^\s*\.include\s+"([^"]+)"\s*$')
DEF_RE = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)::')


def flatten(path, out, seen):
    out.append(path)
    for line in open(path, encoding='utf-8', errors='replace'):
        m = INC_RE.match(line)
        if m and m.group(1) not in seen:
            seen.add(m.group(1))
            flatten(m.group(1), out, seen)


files = []
flatten('data/event_scripts.s', files, set())
print('files pulled in by event_scripts.s:', len(files))

within = {}
across = {}
for path in files:
    body = open(path, encoding='utf-8', errors='replace').read()
    names = DEF_RE.findall(body) if False else re.findall(r'^([A-Za-z_][A-Za-z0-9_]*)::', body, re.M)
    counts = {}
    for n in names:
        counts[n] = counts.get(n, 0) + 1
    for n, c in counts.items():
        if c > 1:
            within.setdefault(n, []).append((path, c, bool(re.search(r'^\s*\.else\b', body, re.M))))
        across.setdefault(n, set()).add(path)

multi_file = {n: sorted(ps) for n, ps in across.items() if len(ps) > 1}

print('labels defined more than once inside one file:', len(within))
for n, entries in sorted(within.items()):
    for path, c, has_else in entries:
        tag = 'assembler .if/.else variant' if has_else else 'REAL DUPLICATE'
        print('   %-45s x%d in %s  [%s]' % (n, c, path, tag))
print('labels defined in more than one file:', len(multi_file))
for n, ps in sorted(multi_file.items())[:20]:
    print('   %s -> %s' % (n, ps))
