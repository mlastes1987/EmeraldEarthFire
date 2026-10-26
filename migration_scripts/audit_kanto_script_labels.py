"""Measure how many Kanto event-script labels an Emerald build can actually resolve.

The Kanto map scripts live inside the ".if IS_FRLG" block of data/event_scripts.s,
but data/maps/events.inc (which registers every map's object/coord/bg events) is
assembled unconditionally. This reports the gap for the Emerald build.
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

# 1. Which .include lines sit inside a conditional block in event_scripts.s?
lines = open('data/event_scripts.s', encoding='utf-8').read().split('\n')
depth = 0
gated = set()
for line in lines:
    s = line.strip()
    if s.startswith(('.if', '.ifdef', '.ifndef')):
        depth += 1
    elif s.startswith('.endif'):
        depth -= 1
    if depth > 0 and s.startswith('.include'):
        m = re.search(r'"([^"]+)"', s)
        if m:
            gated.add(m.group(1))
print('includes inside a conditional block in event_scripts.s:', len(gated))


def labels_in(path):
    try:
        txt = open(path, encoding='utf-8', errors='replace').read()
    except OSError:
        return set()
    return set(re.findall(r'^([A-Za-z_][A-Za-z0-9_]*)::', txt, re.M))


# 2. Labels defined in files that are assembled in every build.
uncond = set()
for root in ('data/maps', 'data/scripts', 'data/text'):
    for dirpath, _, files in os.walk(root):
        for f in files:
            if not f.endswith(('.inc', '.s')):
                continue
            rel = os.path.join(dirpath, f).replace(os.sep, '/')
            if rel in gated:
                continue
            uncond |= labels_in(rel)
for d in ('data', 'src/data'):
    for f in os.listdir(d):
        if f.endswith(('.s', '.inc')):
            uncond |= labels_in(os.path.join(d, f))
print('labels defined in unconditionally assembled files:', len(uncond))

# 3. Script labels referenced by Kanto map events.
kanto = []
for dirpath, _, files in os.walk('data/maps'):
    if 'map.json' not in files:
        continue
    p = os.path.join(dirpath, 'map.json')
    with open(p, encoding='utf-8') as fh:
        d = json.load(fh)
    if d.get('region') == 'REGION_KANTO':
        kanto.append((p, d))
print('Kanto maps:', len(kanto))

refs = set()
for _, d in kanto:
    for key in ('object_events', 'coord_events', 'bg_events'):
        for e in d.get(key) or []:
            v = e.get('script')
            if isinstance(v, str) and re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', v) and v != '0':
                refs.add(v)
missing = sorted(r for r in refs if r not in uncond)
print('distinct script labels referenced by Kanto map events:', len(refs))
print('NOT defined in any unconditionally assembled file:', len(missing))
print('examples:', missing[:10])
