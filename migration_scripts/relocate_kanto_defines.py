#!/usr/bin/env python3
"""Relocate Kanto/Sevii flags and variables into free Emerald id space.

Background
----------
`include/constants/flags.h` and `include/constants/vars.h` are shared by every build.
For a non-FRLG build the "FRLG flags" section of flags.h declares every Kanto flag as
the literal `0`, and `GetFlagPointer(0)` returns NULL, so all Kanto flags are inert
no-ops: hide flags never latch and story flags never persist.

`vars.h` includes `vars_frlg.h` unconditionally, so Kanto map variables do have real
values there - but 37 of the ids they use are also live Hoenn variable ids, so Hoenn
and Kanto map state would overwrite each other in a combined ROM.

This script does two things, both only for non-FRLG builds:

1. Assigns every flag in the "FRLG flags" section a unique id taken from the free
   Emerald flag range (0x500..0x91F), which is far larger than the number of Kanto
   flags, so no save-block size change is needed. The original FireRed id is kept as
   a trailing comment.

2. Emits a `#if !IS_FRLG` remap block in vars.h that moves each Kanto variable whose
   id collides with a Hoenn variable into a free variable slot.

Both operations are deterministic, so re-running the script after editing the map data
or the FRLG constants regenerates exactly the same ids.

Run from the repository root:

    python3 migration_scripts/relocate_kanto_defines.py           # apply
    python3 migration_scripts/relocate_kanto_defines.py --check   # report only
"""

import argparse
import glob
import json
import os
import re
import sys

FLAGS_H = 'include/constants/flags.h'
FLAGS_FRLG_H = 'include/constants/flags_frlg.h'
VARS_H = 'include/constants/vars.h'
VARS_FRLG_H = 'include/constants/vars_frlg.h'

# Free Emerald flag ids live in 0x500..0x91F (the region between the last Emerald hide
# flag and the daily-flag block). FLAGS_COUNT is DAILY_FLAGS_END + 1 == 0x960.
KANTO_FLAG_START = 0x500
KANTO_FLAG_END = 0x91F

# Save-block variable ids live above VARS_START. The end of the range is read from
# vars.h so the script keeps working after the table is extended for a combined
# Hoenn+Kanto build. 0x4000..0x400F are the temporary variables, cleared on every map
# load, so persistent map variables must never be relocated into them.
VARS_START = 0x4000
TEMP_VARS_END = 0x400F


def vars_end():
    """Largest VARS_END defined in vars.h (the non-FRLG value when it is build-dependent)."""
    values = [int(m.group(1), 16)
              for m in re.finditer(r'^#define\s+VARS_END\s+(0x[0-9A-Fa-f]+)\s*$',
                                   read(VARS_H), re.M)]
    if not values:
        sys.exit('error: could not find VARS_END in %s' % VARS_H)
    return max(values)

# Files that *define* these names; they must not count as "references".
DEFINITION_FILES = {FLAGS_H, FLAGS_FRLG_H, VARS_H, VARS_FRLG_H}

VARS_BLOCK_BEGIN = '// --- Kanto/Sevii variable remap (generated) ---'
VARS_BLOCK_END = '// --- end Kanto/Sevii variable remap ---'

SOURCE_EXTS = ('.c', '.h', '.inc', '.s', '.json')


def read(path):
    with open(path, encoding='utf-8') as handle:
        return handle.read()


def read_vars_without_remap():
    """vars.h with the generated remap block removed, so the script sees the original ids."""
    lines = read(VARS_H).split('\n')
    if VARS_BLOCK_BEGIN in lines:
        begin = lines.index(VARS_BLOCK_BEGIN)
        end = lines.index(VARS_BLOCK_END)
        lines = lines[:begin] + lines[end + 1:]
    return '\n'.join(lines)


def write(path, text):
    with open(path, 'w', encoding='utf-8', newline='') as handle:
        handle.write(text)


def parse_defines(path, prefix):
    """Map NAME -> int value for `#define NAME 0x...`, ignoring trailing comments."""
    values = {}
    pattern = re.compile(r'^#define\s+(%s\w+)\s+(0x[0-9A-Fa-f]+)\s*(?://.*)?$' % prefix, re.M)
    for match in pattern.finditer(read(path)):
        values[match.group(1)] = int(match.group(2), 16)
    return values


def kanto_map_dirs():
    dirs = set()
    for path in glob.glob('data/maps/*/map.json'):
        data = json.loads(read(path))
        if data.get('region') == 'REGION_KANTO':
            dirs.add(os.path.dirname(path).replace(os.sep, '/'))
    return dirs


def referenced_names(prefix, skip_files):
    """Every PREFIX_* token referenced outside the files that define them."""
    token = re.compile(r'\b%s_[A-Za-z0-9_]+' % prefix)
    names = set()
    for root in ('src', 'include', 'data', 'test'):
        for dirpath, _, filenames in os.walk(root):
            for filename in filenames:
                if not filename.endswith(SOURCE_EXTS):
                    continue
                path = os.path.join(dirpath, filename).replace(os.sep, '/')
                if path in skip_files:
                    continue
                try:
                    text = read(path)
                except OSError:
                    continue
                names.update(token.findall(text))
    return names


# --------------------------------------------------------------------------- flags

def relocate_flags(check):
    lines = read(FLAGS_H).split('\n')

    try:
        start = lines.index('// FRLG flags') + 1
    except ValueError:
        sys.exit('error: could not find the "// FRLG flags" section in %s' % FLAGS_H)

    end = start
    while not lines[end].startswith('#endif'):
        end += 1

    block = lines[start:end]
    names = []
    for line in block:
        match = re.match(r'^#define\s+(FLAG_\w+)\b', line)
        if match:
            names.append(match.group(1))

    if not names:
        sys.exit('error: no flag definitions found in the FRLG flags section')

    frlg = parse_defines(FLAGS_FRLG_H, 'FLAG')

    # Deterministic order: FireRed id first so the numbering stays traceable to the FRLG
    # constants; names with no FireRed counterpart are appended alphabetically.
    ordered = sorted(names, key=lambda name: (frlg.get(name, 1 << 32), name))

    assigned = {}
    for index, name in enumerate(ordered):
        value = KANTO_FLAG_START + index
        if value > KANTO_FLAG_END:
            sys.exit('error: %d Kanto flags do not fit in 0x%X..0x%X'
                     % (len(names), KANTO_FLAG_START, KANTO_FLAG_END))
        assigned[name] = value

    # Every other flag define in the file must keep a disjoint id.
    other_ids = set()
    for match in re.finditer(r'^#define\s+(FLAG_\w+)\s+0x([0-9A-Fa-f]+)\s*(?://.*)?$',
                             read(FLAGS_H), re.M):
        if match.group(1) in assigned:
            continue
        other_ids.add(int(match.group(2), 16))
    clash = {name: hex(value) for name, value in assigned.items() if value in other_ids}
    if clash:
        sys.exit('error: assigned ids clash with existing Emerald flags: %s' % clash)

    width = max(len(name) for name in names) + 2
    rebuilt = []
    for line in block:
        match = re.match(r'^(#define\s+(FLAG_\w+))\b', line)
        if not match:
            rebuilt.append(line)
            continue
        name = match.group(2)
        comment = ' // FRLG 0x%X' % frlg[name] if name in frlg else ''
        rebuilt.append('#define %s0x%03X%s' % (name.ljust(width), assigned[name], comment))

    if rebuilt == block:
        print('flags: already up to date (%d Kanto flags at 0x%X..0x%X)'
              % (len(names), KANTO_FLAG_START, KANTO_FLAG_START + len(names) - 1))
        return False

    if check:
        print('flags: would assign %d Kanto flags to 0x%X..0x%X'
              % (len(names), KANTO_FLAG_START, KANTO_FLAG_START + len(names) - 1))
        return True

    lines[start:end] = rebuilt
    write(FLAGS_H, '\n'.join(lines))
    print('flags: relocated %d Kanto flags to 0x%X..0x%X'
          % (len(names), KANTO_FLAG_START, KANTO_FLAG_START + len(names) - 1))
    return True


# ---------------------------------------------------------------------------- vars

def parse_defines_text(text, prefix):
    values = {}
    pattern = re.compile(r'^#define\s+(%s\w+)\s+(0x[0-9A-Fa-f]+)\s*(?://.*)?$' % prefix, re.M)
    for match in pattern.finditer(text):
        values[match.group(1)] = int(match.group(2), 16)
    return values


def relocate_vars(check):
    emerald_vars = parse_defines_text(read_vars_without_remap(), 'VAR')
    frlg_vars = parse_defines(VARS_FRLG_H, 'VAR')

    # A name is Kanto-owned if only vars_frlg.h defines it.
    kanto_only = {name for name in frlg_vars if name not in emerald_vars}

    end = vars_end()
    where = {}
    for name in referenced_names('VAR', DEFINITION_FILES):
        value = emerald_vars.get(name, frlg_vars.get(name))
        if value is not None and VARS_START <= value <= end:
            where.setdefault(value, set()).add(name)

    conflicts = {value: names for value, names in where.items() if len(names) > 1}

    moves = {}
    for value, names in sorted(conflicts.items()):
        movable = sorted(name for name in names if name in kanto_only)
        if len(movable) != len(names) - 1:
            sys.exit('error: cannot resolve var conflict at 0x%X cleanly: %s'
                     % (value, sorted(names)))
        moves[movable[0]] = frlg_vars[movable[0]]

    # Free slots are ids no referenced variable claims, excluding the temporary range.
    used = set(where)
    free = [value for value in range(TEMP_VARS_END + 1, end + 1) if value not in used]
    if len(free) < len(moves):
        sys.exit('error: need %d free variable ids but only %d are available'
                 % (len(moves), len(free)))

    # Prefer ids that no defined variable owns at all; fall back to ids of variables that
    # exist but are never referenced. Both are taken from the top of the range downwards,
    # which keeps clear of the low ids that Emerald already uses.
    defined_ids = set(emerald_vars.values()) | set(frlg_vars.values())
    clean = sorted((v for v in free if v not in defined_ids), reverse=True)
    spare = sorted((v for v in free if v in defined_ids), reverse=True)
    pool = clean + spare
    if len(pool) < len(moves):
        sys.exit('error: need %d free variable ids but only %d are available'
                 % (len(moves), len(pool)))
    targets = pool[:len(moves)]
    ordered = sorted(moves.items(), key=lambda item: (item[1], item[0]))
    assignment = {name: targets[index] for index, (name, _) in enumerate(ordered)}

    emitted = [VARS_BLOCK_BEGIN]
    emitted.append('// Kanto/Sevii variables that would otherwise share an id with a Hoenn')
    emitted.append('// variable. Remapped so the two regions keep independent state.')
    emitted.append('// Generated by migration_scripts/relocate_kanto_defines.py')
    emitted.append('#if !IS_FRLG')
    width = max(len(name) for name in assignment) + 2
    for name in sorted(assignment):
        emitted.append('#undef %s' % name)
        emitted.append('#define %s0x%04X // was 0x%04X'
                       % (name.ljust(width), assignment[name], frlg_vars[name]))
    emitted.append('#endif // !IS_FRLG')
    emitted.append(VARS_BLOCK_END)

    lines = read(VARS_H).split('\n')
    if VARS_BLOCK_BEGIN in lines:
        begin = lines.index(VARS_BLOCK_BEGIN)
        end = lines.index(VARS_BLOCK_END)
        current = lines[begin:end + 1]
        if current == emitted:
            print('vars: already up to date (%d remapped variables)' % len(assignment))
            return False
        if check:
            print('vars: would remap %d Kanto variables' % len(assignment))
            return True
        lines[begin:end + 1] = emitted
    else:
        if check:
            print('vars: would remap %d Kanto variables' % len(assignment))
            return True
        include_line = lines.index('#include "constants/vars_frlg.h"')
        lines[include_line + 1:include_line + 1] = [''] + emitted

    write(VARS_H, '\n'.join(lines))
    print('vars: remapped %d Kanto variables' % len(assignment))
    for name in sorted(assignment):
        print('    %-52s 0x%04X -> 0x%04X' % (name, frlg_vars[name], assignment[name]))
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--check', action='store_true',
                        help='report what would change without writing anything')
    args = parser.parse_args()

    if not os.path.exists(FLAGS_H):
        sys.exit('error: run this script from the repository root')

    changed = relocate_flags(args.check)
    changed = relocate_vars(args.check) or changed

    if args.check:
        print('\n%s' % ('changes needed' if changed else 'nothing to do'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
