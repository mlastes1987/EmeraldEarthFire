"""Report which harbour can sail where, per build version.

This tree crosses regions by three different mechanisms, declared in three different
places, which is what makes a routing bug hard to see by reading:

  * the FRLG SEAGALLOP, whose destinations live in the `sSeag` table in
    [seagallop.c](../src/seagallop.c) and are *named* by the scripts as `SEAGALLOP_*` ids
    (they reach `EventScript_SailToDest`, which warps on `VAR_0x8006`);
  * HOENN's own S.S. TIDAL, whose island berths are plain `warp MAP_..., x, y`;
  * the KANTO liner this merge added, which files SEAGALLOP ids too.

Reading any one of those tells you nothing about the others, so this script walks the
*assembled* script text instead: the same `preproc -s | cpp | preproc -ie` pipeline
`audit_script_symbols.py` uses, with the assembler conditionals evaluated. From every
label a `map.json` event names it follows `goto` / `goto_if_*` / `call` / `call_if_*` /
`case` targets, collects the `warp` destinations and `VAR_0x8006` seagallop ids it
reaches, resolves a seagallop id through `sSeag`, and prints the result per origin map.

What it reports, and why each one matters:

  * **destinations** — the table itself: origin map, region, and each destination with
    the mechanism that reaches it. This is the map that was missing.
  * **duplicate islands** — two destination maps whose names differ only by the `_Frlg`
    suffix, both live in the same build. That was the shape of the HOENN/KANTO event-island
    duplication: two complete dungeons holding the *same* legendaries behind *different*
    flags, so a player could catch LUGIA, HO-OH and DEOXYS twice. The HOENN set is gone
    (KANTO_IN_EMERALD.md, "One island, one dungeon"), so this should be silent; it is kept
    because re-adding one of those maps is exactly the mistake it names.
  * **islands no harbour reaches** — a section a live map carries that no harbour
    reaches: a berth that was lost, or a map whose sailor no longer sails it.
  * **stranded piers** — a harbour with no berth back to its own region.

Two limits worth knowing before reading a report. It is a **text-level** check: a label a
`#if !IS_FRLG` block hides is absent, but a script file that merely belongs to a HOENN map
is still read, so an FRLG report lists LILYCOVE's sailors even though `mapjson` drops that
map from an FRLG ROM and nothing can reach them. And it reports the *berths a script names*,
not whether the ticket that buys one is obtainable - the checkout flags are a separate
question (see KANTO_IN_EMERALD.md).

Usage:
    python migration_scripts/audit_harbour_routes.py                 # EMERALD
    python migration_scripts/audit_harbour_routes.py --version frlg
    python migration_scripts/audit_harbour_routes.py --strict        # exit 1 on a finding

Prerequisites: `make -C tools/preproc && make -C tools/mapjson && make -C tools/jsonproc`,
the same three gitignored binaries the symbol audit needs.
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audit_script_symbols as aud  # the pipeline + the conditional evaluator, reused

ROOT = aud.ROOT
MAPS = os.path.join(ROOT, 'data', 'maps')

LABEL = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):\s*$')
BRANCH = re.compile(r'^\s*(goto|goto_if_\w+|call|call_if_\w+|vgoto|vcall|case)\b(.*)$')
WARP = re.compile(r'^\s*warp\s+(MAP_[A-Z0-9_]+)\s*,\s*\d+\s*,\s*\d+')
SET_DEST = re.compile(r'^\s*setvar\s+0x8006\s*,\s*(\d+)\s*$')
SEAG_ROW = re.compile(r'\[\s*(SEAGALLOP_[A-Z0-9_]+)\s*\]\s*=\s*\{\s*MAP_GROUP\((\w+)\)\s*,\s*'
                      r'MAP_NUM\([^)]*\)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*\}')
SEAG_DEFINE = re.compile(r'^\s*#define\s+(SEAGALLOP_[A-Z0-9_]+)\s+(\d+)\s*$', re.M)

# A label is only a berth-seller if the code it reaches actually sails a ship. Without this
# test every `warp` in the game (doors, elevators) and every unrelated `setvar VAR_0x8006`
# (screen shakes take their parameters there) reads as a harbour, which drowns the report.
FERRY_MARKERS = ('DoSeagallopFerryScene',                       # the SEAGALLOP cut-scene
                 'ScriptMenu_CreateLilycoveSSTidalMultichoice')  # HOENN's S.S. TIDAL menu
# The sections the island maps carry, which a harbour must therefore reach. The KANTO twins
# are the live ones: the HOENN `MAPSEC_NAVEL_ROCK` / `MAPSEC_BIRTH_ISLAND` sections were
# deleted with their maps, so they are not listed - a report mentioning them would mean
# someone put those sections back.
ISLAND_SECTIONS = ('MAPSEC_NAVEL_ROCK_FRLG', 'MAPSEC_BIRTH_ISLAND_FRLG',
                   'MAPSEC_FARAWAY_ISLAND', 'MAPSEC_SOUTHERN_ISLAND')


# --------------------------------------------------------------------- the two tables

def split_labels(text):
    """label -> instruction lines, from the flattened script text.

    `preproc -s` has already turned each `Label::` into a `.global Label` plus a plain
    `Label:`, so a definition is a line that is nothing but an identifier and a colon.
    """
    labels = {}
    current = None
    for line in text.split('\n'):
        m = LABEL.match(line)
        if m:
            current = m.group(1)
            labels.setdefault(current, [])
            continue
        if current is not None:
            labels[current].append(line)
    return labels


def seagallop_table():
    """SEAGALLOP id -> (destination map id, x, y), read from src/seagallop.c."""
    src = open(os.path.join(ROOT, 'src', 'seagallop.c'), encoding='utf-8').read()
    header = open(os.path.join(ROOT, 'include', 'constants', 'seagallop.h'), encoding='utf-8').read()
    ids = {name: int(value) for name, value in SEAG_DEFINE.findall(header)}
    table = {}
    for name, map_id, x, y in SEAG_ROW.findall(src):
        if name in ids:
            table[ids[name]] = (map_id, int(x, 16), int(y, 16))
    return table, ids


def map_metadata():
    """map id -> {region, region_map_section, dir, event labels}, and label -> map id."""
    meta = {}
    owner = {}
    for name in sorted(os.listdir(MAPS)):
        path = os.path.join(MAPS, name, 'map.json')
        if not os.path.exists(path):
            continue
        with open(path, encoding='utf-8') as fh:
            data = json.load(fh)
        map_id = data.get('id')
        if not isinstance(map_id, str):
            continue
        labels = set()
        for key in ('object_events', 'coord_events', 'bg_events'):
            for event in data.get(key) or []:
                script = event.get('script')
                if isinstance(script, str) and re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', script):
                    labels.add(script)
        meta[map_id] = {
            'region': data.get('region') or '?',
            'section': data.get('region_map_section') or '?',
            'dir': name,
            'labels': labels,
        }
        for label in labels:
            owner.setdefault(label, map_id)
    return meta, owner


# ------------------------------------------------------------------- walking a script

def reachable(labels, seeds):
    """Every label reachable from the seeds through goto/call/case targets."""
    seen = set()
    stack = [s for s in seeds if s in labels]
    while stack:
        label = stack.pop()
        if label in seen:
            continue
        seen.add(label)
        for line in labels[label]:
            m = BRANCH.match(line)
            if not m:
                continue
            ids = re.findall(r'[A-Za-z_]\w*', m.group(2).split('@')[0])
            if ids and ids[-1] in labels:
                stack.append(ids[-1])
    return seen


def destinations(labels, closure, seag):
    """[(kind, destination map id, detail)] for one sailor's reachable code."""
    found = []
    for label in sorted(closure):
        for line in labels[label]:
            if 'warp' in line:
                m = WARP.match(line)
                if m:
                    found.append(('warp', m.group(1), 'warp %s' % label))
                    continue
            if 'setvar 0x8006' in line:
                m = SET_DEST.match(line)
                if m:
                    dest = int(m.group(1))
                    if dest in seag:
                        found.append(('seagallop', seag[dest][0],
                                      'SEAGALLOP id %d via %s' % (dest, label)))
                    else:
                        found.append(('seagallop', 'SEAGALLOP id %d (no sSeag row)' % dest,
                                      'via %s' % label))
    return found


# ------------------------------------------------------------------------------ report

def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--version', default='emerald', choices=sorted(aud.VERSIONS))
    parser.add_argument('--strict', action='store_true',
                        help='exit 1 when anything below is reported')
    args = parser.parse_args()

    cpp = aud.find_cpp()
    aud.ensure_generated_headers()
    text = aud.run_pipeline(cpp, aud.SRC, args.version, 'routes_%s' % args.version)
    live = aud.evaluate_conditionals(text, aud.VERSIONS[args.version])[0]

    labels = split_labels(live)
    seag, seag_ids = seagallop_table()
    meta, owner = map_metadata()

    reachable_maps = {}
    piers = {}
    for label in sorted(owner):
        if label not in labels:
            continue
        origin = owner[label]
        if origin not in meta:
            continue
        closure = reachable(labels, [label])
        if not any(marker in line
                   for other in closure for line in labels[other]
                   for marker in FERRY_MARKERS):
            continue
        found = destinations(labels, closure, seag)
        if not found:
            continue
        piers.setdefault(origin, []).append((label, found))
        for _, dest, _ in found:
            reachable_maps.setdefault(dest, set()).add(origin)

    print('=== %s: %d harbours, %d berth-selling scripts, %d sSeag rows'
          % (args.version, len(piers), sum(len(v) for v in piers.values()), len(seag)))
    findings = []
    for origin in sorted(piers):
        oregion = meta[origin]['region']
        print('  %s [%s]  (%s)' % (origin, oregion, meta[origin]['dir']))
        inside_region = []
        for label, found in sorted(piers[origin]):
            uniq = sorted({(kind, dest) for kind, dest, _ in found})
            print('      %-46s -> %s'
                  % (label, ', '.join('%s [%s via %s]'
                                      % (dest, meta.get(dest, {}).get('region', '?'), kind)
                                      for kind, dest in uniq) or 'nothing'))
            inside_region += [dest for _, dest, _ in found
                              if meta.get(dest, {}).get('region') == oregion]
        # A harbour is stranded only if *none* of its sailors keeps a berth in its own
        # region -- per script is too strict, since a harbour can run one sailor per sea.
        if not inside_region:
            findings.append('stranded: %s (%s) has no berth inside its own region'
                            % (origin, oregion))

    # Two complete dungeons for one island: same name, `_Frlg` apart, both live.
    for dest in sorted(reachable_maps):
        if not dest.endswith('_FRLG'):
            continue
        pair = dest[:-5]
        if pair in reachable_maps:
            findings.append('duplicate island: %s and %s are both reachable from a harbour '
                            '(%s / %s)'
                            % (pair, dest, ', '.join(sorted(reachable_maps[pair])),
                               ', '.join(sorted(reachable_maps[dest]))))

    # An island a live map carries, with no harbour reaching it: the berth was lost.
    reached_sections = {meta[d]['section'] for d in reachable_maps if d in meta}
    for section in ISLAND_SECTIONS:
        if section in reached_sections:
            continue
        orphans = sorted(m for m, i in meta.items() if i['section'] == section)
        if orphans:
            findings.append('no harbour reaches %s (%d map(s): %s)'
                            % (section, len(orphans), ', '.join(orphans[:4])))
        else:
            findings.append('%s is a listed island section with no map left at all' % section)

    print('  findings: %d' % len(findings))
    for finding in findings:
        print('    - %s' % finding)
    if args.strict and findings:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
