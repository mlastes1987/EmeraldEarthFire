"""Report what an Emerald build would need if the ".if IS_FRLG" block in
data/event_scripts.s were made unconditional.

Lists the composition of the gated include list and any labels that are defined
both inside and outside the block (those would become duplicate definitions).
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

lines = open('data/event_scripts.s', encoding='utf-8').read().split('\n')
depth = 0
gated = []
for line in lines:
    s = line.strip()
    if s.startswith(('.if', '.ifdef', '.ifndef')):
        depth += 1
    elif s.startswith('.endif'):
        depth -= 1
    if depth > 0 and s.startswith('.include'):
        m = re.search(r'"([^"]+)"', s)
        if m:
            gated.append(m.group(1))
print('gated includes:', len(gated))

buckets = {}
for p in gated:
    parts = p.split('/')
    key = 'data/maps/<per-map>' if parts[:2] == ['data', 'maps'] else '/'.join(parts[:2])
    buckets[key] = buckets.get(key, 0) + 1
for k, v in sorted(buckets.items(), key=lambda kv: -kv[1]):
    print('   %-20s %d' % (k, v))


def labels(path):
    try:
        txt = open(path, encoding='utf-8', errors='replace').read()
    except OSError:
        return set()
    return set(re.findall(r'^([A-Za-z_][A-Za-z0-9_]*)::', txt, re.M))


gset = set(gated)
inside = set()
outside = set()
for root in ('data/maps', 'data/scripts', 'data/text'):
    for dirpath, _, files in os.walk(root):
        for f in files:
            if not f.endswith(('.inc', '.s')):
                continue
            p = os.path.join(dirpath, f).replace(os.sep, '/')
            (inside if p in gset else outside).update(labels(p))
for d in ('data', 'src/data'):
    for f in os.listdir(d):
        if f.endswith(('.inc', '.s')):
            outside.update(labels(os.path.join(d, f)))

both = sorted(inside & outside)
print('labels inside gated files:', len(inside))
print('labels in unconditional files:', len(outside))
print('defined in BOTH (collide if un-gated):', len(both))
print('examples:', both[:12])
