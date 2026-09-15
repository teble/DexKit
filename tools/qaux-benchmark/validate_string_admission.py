#!/usr/bin/env python3
"""Validate admission cases against raw fixture rows and complete ordered bytes."""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import subprocess

from validate_string_checks import class_ids


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def string_match(values, atoms, absent):
    for i, (word, mode) in enumerate(atoms):
        word = b'AbsentEveryPool' if absent and i == 0 else word
        if not any(word == value if mode == '=' else value.startswith(word) if mode == '^'
                   else word in value for value in values):
            return False
    return True


def method_match(row, variant, absent):
    name = row['descriptor'].split('->')[1].split('(')[0]
    if variant == 20:
        return name == 'single'  # The other OR branch always fails; there is no root string requirement.
    atoms = [(b'Needle', '*'), (b'LongPrefix/', '*')]
    if variant in (7, 15, 16, 17, 18, 19, 22): atoms = [(b'OnlyOne', '=')]
    if variant == 8: atoms = [(b'SinglePrefix/', '^')]
    if variant == 9: atoms = [(b'ExactlyTwo', '=')]
    if variant == 10: atoms = [(b'PairPrefix/', '^')]
    if variant == 11: atoms = [(b'SameMethodPrefix/', '^')]
    if variant == 12: atoms = [(b'PresentButUnreferenced', '=')]
    if variant == 13: atoms = [(b'AbsentEveryPool', '=')]
    if not string_match(row['values'], atoms, absent): return False
    required_names = {1: 'm00000', 2: 'sharedName', 3: 'm00000', 4: 'sharedName',
                      7: 'single', 8: 'single', 9: 'pairA', 10: 'pairA', 11: 'single',
                      12: 'single', 13: 'single', 16: 'single', 17: 'wrongName'}
    if variant in required_names and name != required_names[variant]: return False
    if variant == 15 and name == 'single': return False
    if variant == 5 and not row['code']: return False
    if variant == 21: return False  # No native methods in this fixture.
    # All returns are void. Public is broad, and empty anyOf is an existing no-op.
    return True


def class_match(owner, members, variant, absent):
    atoms = [(b'OnlyOne', '=')]
    if variant == 1: atoms = [(b'SameMethodPrefix/', '^')]
    if variant in (2, 4): atoms = [(b'OneLargeClass', '=')]
    if variant == 5: atoms = [(b'ExactlyTwo', '='), (b'OnlyOne', '=')]
    if not string_match([v for row in members for v in row['values']], atoms, absent): return False
    if variant == 3 and owner == 'Ladmission/Special;': return False
    if variant == 4 and not any(row['descriptor'].endswith('->m00001()V') for row in members): return False
    return True


def expected(fixture):
    records = json.loads((fixture / 'oracle-rows.json').read_text())
    for row in records: row['values'] = [base64.b64decode(v) for v in row['strings_b64']]
    manifest = json.loads((fixture / 'manifest.json').read_text())
    dex_rows = [[row for row in records if row['dex'] == dex and row['defined']]
                for dex in range(len(manifest['dexes']))]
    dex_classes = []
    for dex, info in enumerate(manifest['dexes']):
        members = {}
        for row in dex_rows[dex]: members.setdefault(row['descriptor'].split('->')[0], []).append(row)
        dex_classes.append([(identity, owner, members.get(owner, []))
                            for identity, owner in class_ids((fixture / info['name']).read_bytes())])
    answer = []
    for classes in (False, True):
        for variant in range(6 if classes else 23):
            for absent in (False, True):
                values, seen = [], set()
                for dex, info in enumerate(manifest['dexes']):
                    rows = dex_rows[dex]
                    if classes:
                        for identity, owner, members in dex_classes[dex]:
                            if class_match(owner, members, variant, absent): values.append([dex, identity, owner])
                    else:
                        for row in rows:
                            if row['descriptor'] not in seen and method_match(row, variant, absent):
                                seen.add(row['descriptor'])
                                values.append([dex, row['id'], row['descriptor']])
                answer.append(dict(classes=classes, variant=variant, absent=absent, values=values))
    return answer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--variant', nargs=2, action='append', metavar=('LABEL', 'EXECUTABLE'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists(): raise SystemExit('Use a new output directory.')
    args.output.mkdir(parents=True)
    oracle = expected(args.fixture)
    (args.output / 'expected.json').write_text(json.dumps(oracle, indent=2) + '\n')
    baseline = None
    results = []
    for label, executable in args.variant:
        destination = args.output / label
        destination.mkdir()
        command = [executable, '--dump', str(args.fixture / 'strings.apk')]
        with (destination / 'stdout.bin').open('wb') as out, (destination / 'stderr.log').open('wb') as err:
            subprocess.run(command, stdout=out, stderr=err, check=True, timeout=300)
        actual = [json.loads(row) for row in re.findall(r'^ADMISSION_RESULTS (.+)$',
                  (destination / 'stderr.log').read_text(), re.M)]
        assert actual == oracle, (label, 'raw row mismatch', [i for i, (a, b) in enumerate(zip(actual, oracle)) if a != b])
        data = (destination / 'stdout.bin').read_bytes()
        assert data
        if baseline is None: baseline = data
        assert baseline == data, (label, 'complete ordered bytes differ')
        results.append(dict(label=label, command=command, executable_sha256=sha(Path(executable)),
                            fixture_sha256=sha(args.fixture / 'strings.apk'),
                            ordered_bytes_sha256=sha(destination / 'stdout.bin'),
                            states=3, cases_per_state=len(oracle), passed=True))
        print('VERIFIED ' + label + ' cases=58 states=3', flush=True)
    (args.output / 'validation.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
