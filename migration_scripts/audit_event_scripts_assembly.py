"""Check the whole event-script tree the way the real build sees it.

data/event_scripts.s is assembled as
    preproc -s  |  cpp  |  preproc -ie  |  as
and `preproc -s` inlines every `.include` before the preprocessor runs. This
script does the same: flatten the `.include` tree, run the C preprocessor over it
in the two build modes, and report duplicate `label::` definitions.

Usage: python3 migration_scripts/audit_event_scripts_assembly.py
"""
import os
import re
import subprocess
import sys
import collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

ROOT_S = 'data/event_scripts.s'
INC_RE = re.compile(r'^\s*\.include\s+"([^"]+)"\s*$')


def flatten(path, out, seen, marker=True):
    """Inline .include directives recursively, like preproc -s does.

    A comment marker records which file each line came from so duplicates can be
    attributed.
    """
    if marker:
        # Assembler comment: survives the preprocessor, which strips C comments.
        out.append('\t@EE_FILE:%s\n' % path)
    for line in open(path, encoding='utf-8', errors='replace'):
        m = INC_RE.match(line)
        if m:
            target = m.group(1)
            if target not in seen:
                seen.add(target)
                flatten(target, out, seen)
            continue
        out.append(line)


_seen_files = set()
buf = []
flatten(ROOT_S, buf, _seen_files)
flat_path = os.path.abspath(os.path.join('.', 'ee_flat_event_scripts.s'))
with open(flat_path, 'w', encoding='utf-8') as fh:
    fh.write(''.join(buf))
print('flattened lines:', len(buf), '->', flat_path)

# Some headers are build outputs (mapjson event_constants, heal-location tables).
# Stub any that are missing so the preprocessor can run; their contents are
# irrelevant to the set of label names.
stub_dir = os.path.abspath('.' + os.sep + 'ee_stub_include')
stubbed = []
for m in re.finditer(r'^\s*#include\s+"([^"]+)"', ''.join(buf), re.M):
    header = m.group(1)
    if os.path.exists(os.path.join('include', header)) or os.path.exists(header):
        continue
    target = os.path.join(stub_dir, *header.split('/'))
    if os.path.exists(target):
        continue
    os.makedirs(os.path.dirname(target), exist_ok=True)
    open(target, 'w', encoding='utf-8').write('/* stub for audit */\n')
    stubbed.append(header)
if stubbed:
    print('stubbed %d missing generated header(s):' % len(stubbed), ', '.join(sorted(stubbed)))

DEF_RE = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)::')
FILE_RE = re.compile(r'^\s*@EE_FILE:(.+?)\s*$')

# Labels that a single file defines twice across assembler .if/.else branches are
# deliberate variants the preprocessor cannot evaluate; collect them up front so
# they are not reported as errors. Hard-coded lists would rot.
BENIGN = set()
for path in sorted(_seen_files):
    body = open(path, encoding='utf-8', errors='replace').read()
    if not re.search(r'^\s*\.else\b', body, re.M):
        continue
    names = re.findall(r'^([A-Za-z_][A-Za-z0-9_]*)::', body, re.M)
    for n in set(names):
        if names.count(n) > 1:
            BENIGN.add(n)

modes = [('emerald', []), ('frlg', ['-DFIRERED'])]
fail = False
for name, extra in modes:
    cmd = ['gcc', '-E', '-x', 'assembler-with-cpp', '-P', '-I', stub_dir, '-I', '.', '-I', 'include'] + extra + [flat_path]
    # Generated headers only exist after a build; stub them on demand and retry.
    for _ in range(20):
        proc = subprocess.run(cmd, capture_output=True)
        stdout = proc.stdout.decode('utf-8', errors='replace')
        stderr = proc.stderr.decode('utf-8', errors='replace')
        if proc.returncode == 0:
            break
        m = re.search(r"fatal error: '([^']+)' file not found", stderr)
        if not m:
            break
        header = m.group(1)
        target = os.path.join(stub_dir, *header.split('/'))
        os.makedirs(os.path.dirname(target), exist_ok=True)
        open(target, 'w', encoding='utf-8').write('/* stub for audit */\n')
        print('   stubbed generated header:', header)
    if proc.returncode != 0:
        print('%s: CPP FAILED (exit %d)' % (name, proc.returncode))
        print('\n'.join(stderr.splitlines()[:20]))
        fail = True
        continue
    counts = collections.Counter()
    where = collections.defaultdict(list)
    current = '?'
    for line in stdout.splitlines():
        fm = FILE_RE.match(line)
        if fm:
            current = fm.group(1)
            continue
        dm = DEF_RE.match(line)
        if dm:
            counts[dm.group(1)] += 1
            where[dm.group(1)].append(current)

    dupes = {k: v for k, v in counts.items() if v > 1}
    real = {}
    benign = {}
    for label, _ in dupes.items():
        if label in BENIGN:
            benign[label] = label
        else:
            real[label] = where[label]

    print('%s: distinct labels=%d  duplicate names=%d (real %d, assembler .if/.else variants %d)'
          % (name, len(counts), len(dupes), len(real), len(benign)))
    for label in sorted(benign):
        print('   benign assembler .if/.else variant: %s' % label)
    for label, files in sorted(real.items()):
        print('   DUPLICATE %s -> %s' % (label, files))
        fail = True
    print('   VermilionCity_EventScript_FerrySailor defined: %d'
          % counts.get('VermilionCity_EventScript_FerrySailor', 0))

# The flattened file and stub headers are scratch; do not leave them behind.
for junk in (flat_path, stub_dir):
    try:
        if os.path.isdir(junk):
            for dp, _, fs in os.walk(junk, topdown=False):
                for f in fs:
                    os.remove(os.path.join(dp, f))
                os.rmdir(dp)
        elif os.path.exists(junk):
            os.remove(junk)
    except OSError as exc:
        print('could not remove %s: %s' % (junk, exc))

sys.exit(1 if fail else 0)
