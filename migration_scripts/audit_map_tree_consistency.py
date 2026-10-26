"""Report a map tree whose JSON does not hang together, and names that no longer resolve.

Deleting or renaming a map leaves debris in four shapes, and none of them is visible by
reading one file:

  * a `map_groups.json` entry with no `data/maps/<name>/map.json` behind it, or a map dir no
    group lists. The second is the quiet one: `mapjson groups` is handed the map files and
    reads the group list as data, so a map nobody lists is *silently dropped from the ROM*;
  * a `map.json` naming a `LAYOUT_*` that `layouts.json` does not define, or a layout whose
    blockdata/border file is missing from `data/layouts/`;
  * an include in `data/event_scripts.s` with no file behind it (what `preproc` fails on, with
    a bare open error), or a `scripts.inc` on disk that no include names - a file nothing
    assembles;
  * a *reference* to a name that nothing defines any more: a `warp MAP_...` in a surviving
    script, a `MAP_GROUP(MAP_...)` in C, a `LOCALID_*` in a special. The constants live in
    generated headers, so a survivor still naming a deleted map only shows up if you look at
    both sides.

The first two are checked and must be clean; so is RETIRED below, the set of names this tree
has deleted on purpose, which must appear nowhere outside KANTO_IN_EMERALD.md. The general
"every MAP_/LOCALID_ token resolves" sweep is available with `--names` but is *not* part of
--strict: those prefixes are also used for names that are not map-tree names at all (the
region map's MAP_INPUT_* / MAP_WIDTH, Makefile variables, and ids an assembler file defines
for itself), so it reports by hand, not as a gate.

Usage:
    python migration_scripts/audit_map_tree_consistency.py            # report
    python migration_scripts/audit_map_tree_consistency.py --strict   # exit 1 on a finding
    python migration_scripts/audit_map_tree_consistency.py --names    # the noisy sweep

`make map-tree-check` is the same thing wired into the build: the ELF takes it as an
order-only prerequisite, so a failing check stops the build before a ROM exists and a passing
one never forces a relink. `make map-tree-check` also works on its own (it builds the tools and
the generated headers it needs).

Prerequisites: run `mapjson groups emerald ...`, `mapjson layouts emerald ...` and
`mapjson event_constants emerald ...` first (or a build) so the headers are current; `make
map-tree-check` does that for you.
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAPS = 'data/maps'
LAYOUTS = 'data/layouts'
GENERATED = {
    'include/constants/map_groups.h',
    'include/constants/layouts.h',
    'include/constants/region_map_sections.h',
    'include/constants/map_event_ids.h',
}
SOURCE_EXT = ('.c', '.h', '.inc', '.s', '.json', '.txt')
GENERATED_MARK = 'auto-generated'

# Map dirs that no group lists, and that no group is expected to list. mapjson only builds the
# maps its group list names, so these are dropped from the ROM: they are the FireRed port's
# leftovers, unreachable before this tree existed. Named here so that a *new* dir left out of a
# group is a finding again.
UNGROUPED_BY_DESIGN = {
    'Route19_UnusedHouse_Frlg',
    'Route23_UnusedHouse',
    'Route6_UnusedHouse_Frlg',
    'SevenIsland_UnusedHouse',
}

# The HOENN Navel Rock / Birth Island set, deleted on purpose once both regions kept a berth to
# the KANTO pair instead (KANTO_IN_EMERALD.md, "One island, one dungeon"). These 24 maps, the
# 10 layouts only they used and the 2 tilesets only they used must not be named anywhere: a
# surviving mention means something still expects a map that is gone.
RETIRED_MAPS = (
    'BirthIsland_Exterior', 'BirthIsland_Harbor',
    'NavelRock_B1F', 'NavelRock_Bottom', 'NavelRock_Down01', 'NavelRock_Down02',
    'NavelRock_Down03', 'NavelRock_Down04', 'NavelRock_Down05', 'NavelRock_Down06',
    'NavelRock_Down07', 'NavelRock_Down08', 'NavelRock_Down09', 'NavelRock_Down10',
    'NavelRock_Down11', 'NavelRock_Entrance', 'NavelRock_Exterior', 'NavelRock_Fork',
    'NavelRock_Harbor', 'NavelRock_Top', 'NavelRock_Up1', 'NavelRock_Up2', 'NavelRock_Up3',
    'NavelRock_Up4',
)
RETIRED_LAYOUTS = (
    'LAYOUT_BIRTH_ISLAND_EXTERIOR', 'LAYOUT_ISLAND_HARBOR', 'LAYOUT_NAVEL_ROCK_B1F',
    'LAYOUT_NAVEL_ROCK_BOTTOM', 'LAYOUT_NAVEL_ROCK_ENTRANCE', 'LAYOUT_NAVEL_ROCK_EXTERIOR',
    'LAYOUT_NAVEL_ROCK_FORK', 'LAYOUT_NAVEL_ROCK_LADDER_ROOM1', 'LAYOUT_NAVEL_ROCK_LADDER_ROOM2',
    'LAYOUT_NAVEL_ROCK_TOP',
)
RETIRED_TILESETS = ('gTileset_NavelRock', 'gTileset_IslandHarbor')

NAMESPACES = {
    'MAP': re.compile(r'\b(MAP_[A-Z0-9_]+)\b'),
    'LAYOUT': re.compile(r'\b(LAYOUT_[A-Z0-9_]+)\b'),
    'MAPSEC': re.compile(r'\b(MAPSEC_[A-Z0-9_]+)\b'),
    'LOCALID': re.compile(r'\b(LOCALID_[A-Z0-9_]+)\b'),
}


def repo_file(path):
    return os.path.join(ROOT, path)


def read(path):
    with open(repo_file(path), encoding='utf-8') as f:
        return f.read()


def is_generated(text):
    return GENERATED_MARK in text[:600]


def walk_sources(include_text=True):
    for base, dirs, files in os.walk(ROOT):
        dirs[:] = [d for d in dirs if d not in ('.git', 'build', '__pycache__', 'm', 'tools')]
        for f in files:
            path = os.path.relpath(os.path.join(base, f), ROOT).replace(os.sep, '/')
            if path in GENERATED or path.endswith('.md') or path.startswith('migration_scripts/'):
                continue
            if f.endswith(SOURCE_EXT if include_text else ('.json', '.txt')):
                yield path


# --------------------------------------------------------------- the tree's own JSON

def map_dirs():
    out = {}
    for name in sorted(os.listdir(repo_file(MAPS))):
        path = os.path.join(MAPS, name, 'map.json')
        if os.path.exists(repo_file(path)):
            out[name] = json.loads(read(path))
    return out


def groups():
    data = json.loads(read(os.path.join(MAPS, 'map_groups.json')))
    listed = {}
    for group in data['group_order']:
        for name in data.get(group, []):
            listed.setdefault(name, group)
    return listed


def json_findings():
    findings = []
    dirs = map_dirs()
    listed = groups()

    for name in sorted(set(listed) - set(dirs)):
        findings.append('%s lists %s, but %s/map.json does not exist'
                        % (os.path.join(MAPS, 'map_groups.json'), name, os.path.join(MAPS, name)))
    for name in sorted(set(dirs) - set(listed)):
        if name in UNGROUPED_BY_DESIGN:
            continue
        findings.append('%s/map.json is in no group of %s (mapjson will drop it)'
                        % (os.path.join(MAPS, name), os.path.join(MAPS, 'map_groups.json')))

    ids = {}
    for name, data in dirs.items():
        ids.setdefault(data['id'], []).append(name)
    for map_id, names in sorted(ids.items()):
        if len(names) > 1:
            findings.append('%s is the id of %s' % (map_id, ' and '.join(sorted(names))))

    layouts = json.loads(read(os.path.join(LAYOUTS, 'layouts.json')))
    by_id = {l['id']: l for l in layouts['layouts']}
    for name, data in sorted(dirs.items()):
        layout = data.get('layout')
        if layout not in by_id:
            findings.append('%s names layout %s, which layouts.json does not define'
                            % (name, layout))
            continue
        for key in ('blockdata_filepath', 'border_filepath'):
            path = by_id[layout].get(key)
            if path and not os.path.exists(repo_file(path)):
                findings.append('%s: %s of %s is missing from disk' % (name, key, layout))
    return findings


MAP_INCLUDE = re.compile(r'^\s*\.include\s+"(data/maps/[^"]+)"', re.M)


def include_findings():
    """`data/event_scripts.s` names every map's script file, so both directions matter there."""
    findings = []
    includes = MAP_INCLUDE.findall(read('data/event_scripts.s'))
    for path in includes:
        if not os.path.exists(repo_file(path)):
            findings.append('data/event_scripts.s includes %s, which does not exist' % path)
    named = {p.split('/')[2] for p in includes}
    for name in sorted(os.listdir(repo_file(MAPS))):
        if name in named or name in UNGROUPED_BY_DESIGN:
            continue
        if os.path.exists(repo_file(os.path.join(MAPS, name, 'scripts.inc'))):
            findings.append('%s/scripts.inc is not included by data/event_scripts.s, so nothing '
                            'assembles it' % os.path.join(MAPS, name))
    return findings


def retired_findings():
    """The retired set must be gone from the tree, both as a definition and as a use.

    One alternation, not one pattern per name: this walks every source line in the tree, and a
    build gate has to be cheap enough to sit in front of the link.
    """
    names = sorted(set(RETIRED_LAYOUTS) | set(RETIRED_MAPS) | set(RETIRED_TILESETS))
    combined = re.compile(r'\b(?:%s)\b' % '|'.join(re.escape(n) for n in names))
    frlg_twin = {n: re.compile(r'%s_FRLG\b' % re.escape(n)) for n in RETIRED_LAYOUTS}
    findings = []
    for path in walk_sources():
        try:
            text = read(path)
        except (UnicodeDecodeError, OSError):
            continue
        for lineno, line in enumerate(text.split('\n'), 1):
            for name in set(combined.findall(line)):
                # A layout id carries the FRLG suffix for the surviving twin and is matched
                # whole-word above, so only the bare retired name is a real hit.
                if name in frlg_twin and frlg_twin[name].search(line):
                    continue
                findings.append('%s:%d still names %s' % (path, lineno, name))
    return findings


# ---------------------------------------------------------------- informational sweep

def header_names():
    names = {k: set() for k in NAMESPACES}
    patterns = (r'^#define\s+([A-Z][A-Z0-9_]+)', r'^\s*([A-Z][A-Z0-9_]+)\s*[,=]',
                r'^\s*\.set\s+([A-Z][A-Z0-9_]+)')
    for base in ('include', 'asm', 'data'):
        for dirpath, dirs, files in os.walk(repo_file(base)):
            dirs[:] = [d for d in dirs if d != '__pycache__']
            for f in files:
                if not f.endswith(('.h', '.inc', '.s')):
                    continue
                try:
                    text = open(os.path.join(dirpath, f), encoding='utf-8').read()
                except (UnicodeDecodeError, OSError):
                    continue
                if is_generated(text):
                    continue
                for pattern in patterns:
                    for name in re.findall(pattern, text, re.M):
                        for namespace in NAMESPACES:
                            if name.startswith(namespace + '_'):
                                names[namespace].add(name)
    return names


def defined_side():
    defined = {
        'MAP': set(re.findall(r'^\s*(MAP_[A-Z0-9_]+)\s*=', read('include/constants/map_groups.h'), re.M)),
        'LAYOUT': set(re.findall(r'^#define\s+(LAYOUT_[A-Z0-9_]+)',
                                 read('include/constants/layouts.h'), re.M)),
        'MAPSEC': set(re.findall(r'^\s*(MAPSEC_[A-Z0-9_]+)\s*[,=]',
                                 read('include/constants/region_map_sections.h'), re.M)),
        'LOCALID': set(re.findall(r'^#define\s+(LOCALID_[A-Z0-9_]+)',
                                  read('include/constants/map_event_ids.h'), re.M)),
    }
    from_headers = header_names()
    for namespace in defined:
        defined[namespace] |= from_headers[namespace]
    return defined


def name_findings():
    defined = defined_side()
    findings = []
    for path in sorted(walk_sources()):
        try:
            text = read(path)
        except (UnicodeDecodeError, OSError):
            continue
        if is_generated(text):
            continue
        for lineno, line in enumerate(text.split('\n'), 1):
            for namespace, pattern in NAMESPACES.items():
                for name in pattern.findall(line):
                    # A name ending in '_' is a macro parameter being concatenated
                    # (`.macro map_header ..., script` writes `MAP_SCRIPT_\script:`).
                    if name.endswith('_') or name in defined[namespace]:
                        continue
                    findings.append('%s:%d: %s is defined by no header or macro'
                                    % (path, lineno, name))
    return findings


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--strict', action='store_true', help='exit 1 on any finding')
    parser.add_argument('--names', action='store_true',
                        help='also run the noisy every-name sweep (not part of --strict)')
    args = parser.parse_args()

    for path in GENERATED:
        if not os.path.exists(repo_file(path)):
            sys.exit('%s is missing: run mapjson (or a build) first' % path)

    dirs = map_dirs()
    json_issues = json_findings()
    include_issues = include_findings()
    retired_issues = retired_findings()

    print('map dirs=%d  grouped=%d  retired names=%d'
          % (len(dirs), len(groups()), len(RETIRED_MAPS) + len(RETIRED_LAYOUTS) + len(RETIRED_TILESETS)))
    print('json findings: %d' % len(json_issues))
    for line in json_issues[:40]:
        print('  ' + line)
    print('script include findings: %d' % len(include_issues))
    for line in include_issues[:40]:
        print('  ' + line)
    print('retired names still referenced: %d' % len(retired_issues))
    for line in retired_issues[:40]:
        print('  ' + line)

    findings = json_issues + include_issues + retired_issues
    if args.names:
        sweep = name_findings()
        print('every-name sweep (informational): %d' % len(sweep))
        for line in sweep[:40]:
            print('  ' + line)

    if args.strict and findings:
        sys.exit(1)
    print('OK' if not findings else 'FINDINGS')


if __name__ == '__main__':
    main()
