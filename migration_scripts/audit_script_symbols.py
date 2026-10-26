"""Resolve every symbol the Emerald assembler sees in the event-script data.

The build assembles scripts as

    preproc -s <src> charmap.txt | cpp -iquote include -DMODERN=1 -D<GAME> - | preproc -ie <src> charmap.txt | as

`preproc -e` expands the C `enum` bodies pulled in by the header includes into
`.global X; .equiv X, (base) + n`, so after the second `preproc` pass every constant
a script can name has a definition in the text handed to `as`.

Two things the *preprocessor* cannot decide are left for `as`:

  * `.if IS_FRLG` / `.endif` blocks. cpp substitutes IS_FRLG (0 in an Emerald build),
    so those become `.if 0`; the script labels inside them never reach the object file.
  * `name = value` assignments and `--defsym` symbols used in `.if` expressions.

This script replays the pipeline and then evaluates the assembler conditionals, which
is what makes the result meaningful: the FRLG script block is *text* in the
preprocessor output in both trees, and only becomes absent from the Emerald build when
`.if` is honoured.

It then reports, for one build version:

  * UPPER_SNAKE constants referenced in the live text but never defined  ("undefined
    constants" would reach `as` as undefined symbols), diffed against `HEAD` so
    regressions stand out;
  * script labels named by the map event data (`data/maps/*/map.json`) that no live
    definition covers -- the failure mode that un-gating the FRLG block had to fix.

Usage:
    python3 migration_scripts/audit_script_symbols.py                      # emerald + HEAD diff
    python3 migration_scripts/audit_script_symbols.py --version frlg
    python3 migration_scripts/audit_script_symbols.py --no-diff

Prerequisites: `make -C tools/preproc && make -C tools/mapjson && make -C tools/jsonproc`
(the script regenerates include/constants/{map_event_ids,heal_locations,script_commands}.h
if the binaries exist).
"""
import argparse
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

# Only the event-script tree needs to be walked: data/map_events.s references map script
# labels but never defines one, so a label it names is covered exactly when
# event_scripts.s defines it.
SRC = 'data/event_scripts.s'
TU_SOURCES = [SRC]
CHARMAP = 'charmap.txt'
VERSIONS = {'emerald': 'EMERALD', 'frlg': 'FIRERED', 'leafgreen': 'LEAFGREEN'}
EXE = '.exe' if os.name == 'nt' else ''
PREPROC = os.path.join(ROOT, 'tools', 'preproc', 'preproc' + EXE)
MAPJSON = os.path.join(ROOT, 'tools', 'mapjson', 'mapjson' + EXE)
JSONPROC = os.path.join(ROOT, 'tools', 'jsonproc', 'jsonproc' + EXE)

SCRATCH = os.path.join(os.path.expanduser('~'), '.ee_script_audit')

TOKEN = re.compile(r'\b[A-Z][A-Z0-9_]{2,}\b')
IDENT = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\b')
# `preproc -e` writes `.global X; .equiv X, ((12 | (37 << 8))) + 0` on one line, and its
# value can be a compound expression, so match the name anywhere on the line rather than
# anchoring to the directive or requiring a literal.
EQUIV = re.compile(r'\.equiv\s+([A-Za-z_][A-Za-z0-9_]*)\s*,')
# `.equ`/`.set` definitions, whatever their right-hand side is: the map scripts use them
# heavily for per-map aliases (`.equ SIGN_LADY_READY, VAR_TEMP_2`).
DIRECTIVE_DEF = re.compile(r'\.(?:equ|set|setvar)\s+([A-Za-z_][A-Za-z0-9_]*)\s*,')
ASSIGN = re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([0-9]+)\s*$', re.M)
LABEL = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):', re.M)
# Every `.if*` form counts as an opener: .ifdef/.ifndef/.ifc/.ifeq/.ifnb/.ifgt ...
COND = re.compile(r'^\s*\.(if[a-z]*|elseif|else|endif)\b(.*)$')
# `preproc -s` emits `.ifdef LABEL ; .size LABEL, . - LABEL ; .endif` on one line.
# The whole directive is self contained (and sometimes carries a trailing `.global`),
# so it must be dropped rather than counted as an opener with no matching `.endif`.
SIZE_IDIOM = re.compile(r'^\s*\.ifdef\s+\S+\s*;\s*\.size\b')
DEFSYMS = {'MODERN': 1}


# --------------------------------------------------------------------------- setup

def find_cpp():
    candidates = ([os.environ['CPP']] if os.environ.get('CPP') else []) + \
                 ['arm-none-eabi-cpp', 'cpp', 'clang', 'gcc']
    for cand in candidates:
        path = cand if os.path.isabs(cand) else None
        if path is None:
            import shutil
            path = shutil.which(cand)
        if not path:
            continue
        probe = subprocess.run([path, '-E', '-iquote', 'include', '-DMODERN=1', '-DTESTING=0',
                                '-DEMERALD', '-x', 'assembler-with-cpp', '-'],
                               input=b'', capture_output=True)
        if probe.returncode == 0:
            return path
    sys.exit('no usable C preprocessor found (tried arm-none-eabi-cpp, cpp, clang, gcc)')


def ensure_generated_headers():
    """Generate the build outputs the script pipeline needs, if they are missing."""
    if not os.path.exists('include/constants/map_event_ids.h'):
        if not os.path.exists(MAPJSON):
            sys.exit('include/constants/map_event_ids.h is missing and %s is not built' % MAPJSON)
        # Windows caps the command line length, so run from data/maps with short arguments
        # (the program itself must still be an absolute path: CreateProcess will not
        # resolve a relative one).
        names = sorted(n for n in os.listdir('data/maps')
                       if os.path.exists('data/maps/%s/map.json' % n))
        subprocess.run([MAPJSON, 'event_constants', 'emerald'] +
                       [n + '/map.json' for n in names] +
                       ['../../include/constants/map_event_ids.h'],
                       cwd='data/maps', capture_output=True, check=True)
    if os.path.exists(JSONPROC) and not os.path.exists('include/constants/heal_locations.h'):
        subprocess.run([JSONPROC, 'src/data/heal_locations.json',
                        'src/data/heal_locations.constants.json.txt',
                        'include/constants/heal_locations.h'], capture_output=True)
    if not os.path.exists('include/constants/script_commands.h'):
        subprocess.run([sys.executable, 'tools/misc/make_scr_cmd_constants.py'], capture_output=True)


# ------------------------------------------------------------------- the pipeline

def run_pipeline(cpp, src, version, tag):
    """preproc -s | cpp | preproc -ie, exactly as the Makefile does it."""
    os.makedirs(SCRATCH, exist_ok=True)
    flat = os.path.join(SCRATCH, tag + '.flat.s')
    final = os.path.join(SCRATCH, tag + '.final.s')

    with open(flat, 'wb') as out:
        step1 = subprocess.run([PREPROC, '-s', src, CHARMAP], stdout=out, stderr=subprocess.PIPE)
    if step1.returncode != 0:
        sys.exit('preproc -s failed on %s: %s' % (src, step1.stderr.decode('utf-8', 'replace')[:2000]))

    cmd = [cpp, '-E', '-x', 'assembler-with-cpp', '-iquote', 'include', '-DMODERN=1',
           '-DTESTING=0', '-D' + VERSIONS[version], flat]
    step2 = subprocess.run(cmd, capture_output=True)
    if step2.returncode != 0:
        sys.exit('cpp failed on %s:\n%s' % (flat, step2.stderr.decode('utf-8', 'replace')[:4000]))

    with open(final, 'wb') as out:
        step3 = subprocess.run([PREPROC, '-ie', src, CHARMAP], input=step2.stdout,
                               stdout=out, stderr=subprocess.PIPE)
    if step3.returncode != 0:
        sys.exit('preproc -ie failed on %s: %s' % (src, step3.stderr.decode('utf-8', 'replace')[:2000]))
    return open(final, encoding='utf-8', errors='replace').read()


# ------------------------------------------------------- assembler conditionals

def _to_python(expr):
    """Translate a GNU as `.if` expression into a Python expression."""
    expr = re.sub(r'\b0[xX]([0-9a-fA-F]+)\b', lambda m: str(int(m.group(0), 16)), expr)
    expr = expr.replace('&&', ' and ').replace('||', ' or ')
    expr = re.sub(r'!\s*(?!=)', ' not ', expr)
    return expr


def evaluate_conditionals(text, extra_defs):
    """Return the text `as` would keep, honouring .if/.elseif/.else/.endif.

    cpp has already substituted the headers' macros, so what is left in a directive is
    either a number or an assembler symbol (`--defsym` or a `name = value` assignment).
    Unknown symbols evaluate to 0, which is what as does.
    """
    defs = dict(DEFSYMS)
    defs[extra_defs] = 1
    # Only literal assignments matter for `.if` conditions; symbolic ones are not numbers.
    for name, value in ASSIGN.findall(text):
        defs[name] = int(value)

    def truth(expr):
        expr = expr.split('//')[0].split('@')[0].strip()
        if not expr:
            return False
        py = _to_python(expr)
        for name in set(IDENT.findall(py)):
            if name in ('and', 'or', 'not'):
                continue
            py = re.sub(r'\b%s\b' % name, str(defs.get(name, 0)), py, count=0)
        try:
            return bool(eval(py, {'__builtins__': {}}, {}))
        except Exception:
            return False

    out = []
    stack = []           # (parent_active, taken, active)
    active = True
    unevaluated = 0
    macro_depth = 0      # `.if` inside a .macro body is text until the macro expands
    for line in text.split('\n'):
        if SIZE_IDIOM.match(line):
            continue
        if re.match(r'^\s*\.macro\b', line):
            macro_depth += 1
        elif re.match(r'^\s*\.endm\b', line):
            macro_depth = max(0, macro_depth - 1)
        m = COND.match(line) if macro_depth == 0 else None
        if not m:
            if active:
                out.append(line)
            continue
        kind, rest = m.group(1), m.group(2)
        if kind.startswith('if'):
            parent = active
            if not parent:
                stack.append((False, True, False))
                continue
            if kind == 'ifdef':
                cond = rest.strip() in defs
            elif kind == 'ifndef':
                cond = rest.strip() not in defs
            else:
                cond = truth(rest)
            stack.append((parent, cond, cond))
            active = cond
        elif kind == 'elseif':
            if not stack:
                continue
            parent, taken, _ = stack.pop()
            cond = (not taken) and truth(rest) if parent else False
            stack.append((parent, taken or cond, cond))
            active = cond
        elif kind == 'else':
            if not stack:
                continue
            parent, taken, _ = stack.pop()
            cond = parent and not taken
            stack.append((parent, True, cond))
            active = cond
        else:  # endif
            if stack:
                parent, _, _ = stack.pop()
                active = parent
    if stack:
        unevaluated = len(stack)
    return '\n'.join(out), unevaluated


def split_defs_refs(text):
    """Definitions and constant-like references from live assembly text.

    `.macro` bodies are skipped: `as` only substitutes their text when the macro is
    expanded, so a name appearing there (`.byte SCR_OP_NOP`, `\flag == FALSE`) is not a
    reference the object file necessarily contains, and `name = value` inside a body is
    not a definition until then either.
    """
    top = []
    macro_depth = 0
    for line in text.split('\n'):
        if SIZE_IDIOM.match(line):
            continue
        if re.match(r'^\s*\.macro\b', line):
            macro_depth += 1
            continue
        if re.match(r'^\s*\.endm\b', line):
            macro_depth = max(0, macro_depth - 1)
            continue
        if macro_depth == 0:
            top.append(line)
    top = '\n'.join(top)

    defined = set(EQUIV.findall(top))
    defined |= set(DIRECTIVE_DEF.findall(top))
    defined |= set(n for n, _ in ASSIGN.findall(top))
    defined |= set(LABEL.findall(top))

    body = []
    for line in top.split('\n'):
        s = line.strip()
        if not s or s.startswith('#'):
            continue
        if s.startswith('.'):
            # Data directives still carry symbol references; dialogue does not.
            if not re.match(r'^\.(2byte|4byte|word|byte|space|short|long)\b', s):
                continue
        body.append(re.sub(r'@.*$', '', s))
    refs = set(TOKEN.findall('\n'.join(body)))
    return defined, refs


def map_event_script_labels(region=None):
    """Script labels named by map event data, which the event objects must resolve."""
    labels = set()
    for dirpath, _, files in os.walk('data/maps'):
        if 'map.json' not in files:
            continue
        with open(os.path.join(dirpath, 'map.json'), encoding='utf-8') as fh:
            data = json.load(fh)
        if region and data.get('region') != region:
            continue
        for key in ('object_events', 'coord_events', 'bg_events'):
            for event in data.get(key) or []:
                script = event.get('script')
                if isinstance(script, str) and re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', script):
                    labels.add(script)
    return labels


def analyse(cpp, src, version, tag):
    """Symbols an Emerald/FireRed build would see for one event_scripts.s revision."""
    live = []
    for tu in TU_SOURCES:
        source = src if tu == SRC else tu
        text = run_pipeline(cpp, source, version, '%s_%s' % (tag, os.path.basename(tu).replace('.', '_')))
        active, unbalanced = evaluate_conditionals(text, VERSIONS[version])
        live.append(active)
        if unbalanced:
            print('   note: %s left %d unclosed .if block(s)' % (source, unbalanced))
    defined = set()
    refs = set()
    for text in live:
        d, r = split_defs_refs(text)
        defined |= d
        refs |= r
    return defined, refs


def report_unresolved(label, defined, refs, limit=25):
    missing = sorted(refs - defined)
    print('%s: definitions=%d  referenced constants=%d  UNRESOLVED=%d'
          % (label, len(defined), len(refs), len(missing)))
    for tok in missing[:limit]:
        print('   %s' % tok)
    if len(missing) > limit:
        print('   ... and %d more' % (len(missing) - limit))
    return set(missing)


def head_source():
    blob = subprocess.run(['git', 'show', 'HEAD:' + SRC], capture_output=True)
    if blob.returncode != 0:
        sys.exit('could not read HEAD:%s (use --no-diff outside a git checkout)' % SRC)
    os.makedirs(SCRATCH, exist_ok=True)
    path = os.path.join(SCRATCH, 'event_scripts.head.s')
    with open(path, 'wb') as fh:
        fh.write(blob.stdout)
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--version', choices=sorted(VERSIONS), default='emerald')
    ap.add_argument('--no-diff', action='store_true')
    args = ap.parse_args()

    if not os.path.exists(PREPROC):
        sys.exit('%s is missing; build it first (make -C tools/preproc)' % PREPROC)
    ensure_generated_headers()
    cpp = find_cpp()
    print('preprocessor: %s' % cpp)
    print('sources: %s' % ', '.join(TU_SOURCES))
    print()

    refs_wanted = map_event_script_labels()
    kanto_wanted = map_event_script_labels(region='REGION_KANTO')
    print('script labels named by map event data: %d (%d of them Kanto)'
          % (len(refs_wanted), len(kanto_wanted)))
    print()

    defined, refs = analyse(cpp, SRC, args.version, args.version + '_new')
    missing = report_unresolved('%s / working tree' % args.version, defined, refs)

    uncovered = sorted(refs_wanted - defined)
    print('script labels no live definition covers: %d (Kanto-only: %d)'
          % (len(uncovered), len([l for l in uncovered if l in kanto_wanted])))
    for label_name in uncovered[:15]:
        print('   %s' % label_name)
    if len(uncovered) > 15:
        print('   ... and %d more' % (len(uncovered) - 15))

    if args.no_diff:
        return 1 if (missing or uncovered) else 0

    print()
    head = head_source()
    head_defined, head_refs = analyse(cpp, head, args.version, args.version + '_head')
    head_missing = report_unresolved('%s / HEAD' % args.version, head_defined, head_refs)
    head_uncovered = sorted(refs_wanted - head_defined)
    head_kanto = [l for l in head_uncovered if l in kanto_wanted]
    print('script labels no live definition covers: %d (Kanto-only: %d)'
          % (len(head_uncovered), len(head_kanto)))

    print()
    print('=== delta vs HEAD ===')
    introduced = sorted(missing - head_missing)
    fixed = sorted(head_missing - missing)
    print('newly unresolved constants: %d' % len(introduced))
    for tok in introduced[:20]:
        print('   %s' % tok)
    print('no longer unresolved constants: %d' % len(fixed))
    print('map-event labels now covered: %d' % (len(head_uncovered) - len(uncovered)))
    return 1 if introduced else 0


if __name__ == '__main__':
    sys.exit(main())
