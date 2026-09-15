#!/usr/bin/env python3
"""Independent fixture predicates plus complete ordered batch result bytes."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

from validate_string_checks import FOLD, LONG, class_ids

QUERY_COUNT = 16


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def groups(variant):
    # Atoms are raw bytes, predicate kind, and ASCII-fold flag. These predicates
    # operate on the separately encoded fixture rows, without using native AC.
    def atom(value, kind='equal', fold=False): return (value, kind, fold)
    needle, second = atom(b'Needle'), atom(b'Second')
    if variant == 0: return []
    if variant == 1: return [('empty', [])]
    if variant == 2: return [('empty', []), ('needle', [atom(b'Needle', 'contains')]), ('absent', [atom(b'AbsentEveryPool', 'contains')])]
    if variant == 3:
        n, short, s = atom(b'Needle', 'contains'), atom(b'Need', 'contains'), atom(b'Second', 'contains')
        return [('z-two', [n, s]), ('a-need', [short]), ('duplicate', [n, n]), ('overlap', [short, n]),
                ('miss', [n, atom(b'AbsentEveryPool', 'contains')])]
    if variant == 4: return [('empty-value', [atom(b'')]), ('equal', [needle]), ('both', [needle, second])]
    if variant == 5: return [('prefix', [atom(b'Needle', 'prefix')]), ('long', [atom(LONG, 'prefix')]),
                             ('overlap', [atom(b'Needle', 'prefix'), atom(b'Need', 'prefix')])]
    if variant == 6: return [('suffix', [atom(b'Needle', 'suffix')]), ('two', [atom(b'Needle', 'suffix'), atom(b'Second', 'suffix')])]
    if variant == 7: return [('exact', [needle]), ('prefix', [atom(b'Second', 'prefix')]), ('suffix', [atom(b'tail', 'suffix')])]
    if variant == 8:
        n = atom(b'needle', 'contains', True)
        return [('fold', [n]), ('both', [n, atom(b'second', 'contains', True)])]
    if variant == 9: return [('nul', [atom(b'Needle\xc0\x80tail')]), ('unicode', [atom(b'\xce\xbb', 'contains')]),
                             ('surrogate', [atom(b'\xed\xa0\x80')]), ('supplementary', [atom(b'\xed\xa0\xbd\xed\xb8\x80')]),
                             ('del', [atom(b'\x7f', 'contains')])]
    if variant == 10: return [('same', [needle]), ('other', [needle]), ('same', [second])]
    if variant == 11: return [('or', [(b'', 'or', False)]), ('empty', []), ('null-list', None), ('plain', [second])]
    if variant == 12: return [('or', [(b'', 'or', False)]), ('any-value', [(b'', 'any', False)])]
    if variant in (13, 14): return [('equal', [needle]), ('empty', [])]
    if variant == 15: return [('', [needle]), ('z', [needle]), ('', [needle])]
    raise AssertionError(variant)


def matched(rows, atom):
    value, kind, fold = atom
    if kind == 'or': return b'Needle' in rows or b'OnlySecond' in rows
    if kind == 'any': return bool(rows)
    if fold: value, rows = value.translate(FOLD), [row.translate(FOLD) for row in rows]
    if kind == 'equal': return value in rows
    if kind == 'contains': return any(value in row for row in rows)
    if kind == 'prefix': return any(row.startswith(value) for row in rows)
    if kind == 'suffix': return any(row.endswith(value) for row in rows)
    raise AssertionError(kind)


def code_methods(data):
    count, offset = struct.unpack_from('<2I', data, 96)
    present = set()
    def uleb(position):
        value, shift = 0, 0
        while True:
            byte = data[position]; position += 1
            value |= (byte & 127) << shift
            if not byte & 128: return value, position
            shift += 7
    for i in range(count):
        position = struct.unpack_from('<I', data, offset + i * 32 + 24)[0]
        if not position: continue
        counts = []
        for _ in range(4):
            n, position = uleb(position); counts.append(n)
        for _ in range(2 * (counts[0] + counts[1])): _, position = uleb(position)
        for n in counts[2:]:
            identity = 0
            for _ in range(n):
                delta, position = uleb(position); identity += delta
                _, position = uleb(position)
                code, position = uleb(position)
                if code: present.add(identity)
    return present


def expected(fixture):
    manifest = json.loads((fixture / 'manifest.json').read_text())
    independent = json.loads((fixture / 'oracle-rows.json').read_text())
    answer = {}
    for classes in (False, True):
        for variant in range(QUERY_COUNT):
            requested = groups(variant)
            result = {key: [] for key, _ in requested}
            # Simple keyword batches merge duplicate keys and deduplicate atoms.
            # An empty group is absent from keywords_map unless all groups are
            # empty, which chooses the existing direct fallback instead.
            merged = {}
            composite = variant in (11, 12)
            if not composite:
                for key, atoms in requested:
                    for atom in atoms: merged.setdefault(key, set()).add(atom)
            for dex, (info, record) in enumerate(zip(manifest['dexes'], independent)):
                raw = (fixture / info['name']).read_bytes()
                code = code_methods(raw)
                rows = {key: [base64.b64decode(v) for v in values] for key, values in record['method_rows'].items()}
                by_class = {name: [] for name in record['classes']}
                owners = set()
                for descriptor, values in rows.items():
                    owner = descriptor.split('->')[0]; owners.add(owner); by_class[owner].extend(values)
                items = class_ids(raw) if classes else [(m['id'], m['descriptor']) for m in info['methods'] if m['id'] in code]
                for identity, descriptor in items:
                    if variant in (13, 14): continue
                    if classes and descriptor not in owners: continue
                    values = by_class[descriptor] if classes else rows[descriptor]
                    if merged:
                        candidates = [(key, atoms) for key, atoms in sorted(merged.items()) if all(matched(values, atom) for atom in atoms)]
                    else:
                        candidates = [(key, atoms) for key, atoms in requested if atoms is None or all(matched(values, atom) for atom in atoms)]
                    for key, _ in candidates: result[key].append([dex, identity, descriptor])
            answer[(classes, variant)] = [[key, values] for key, values in sorted(result.items())]
    return answer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--variant', nargs=2, action='append', required=True, metavar=('LABEL', 'ARTIFACT'))
    parser.add_argument('--reuse-control', type=Path)
    args = parser.parse_args()
    assert not args.output.exists() or not any(args.output.iterdir())
    assert len({label for label, _ in args.variant}) == len(args.variant)
    args.output.mkdir(parents=True, exist_ok=True)
    oracle, baseline, records = expected(args.fixture), None, []
    for index, (label, artifact) in enumerate(args.variant):
        artifact = Path(artifact)
        executable = artifact / 'build/Core/dexkit_batch_checks'
        manifest = json.loads((artifact / 'artifact.json').read_text())
        assert sha(artifact / 'libdexkit.dylib') == manifest['native_sha256']
        destination = args.output / label; destination.mkdir()
        command = [str(executable), '--dump', str(args.fixture / 'strings.apk')]
        if index == 0 and args.reuse_control:
            prior = json.loads((args.reuse_control.parent / 'validation.json').read_text())
            assert prior['fixture_sha256'] == sha(args.fixture / 'strings.apk')
            old = [r for r in prior['records'] if r['label'] == args.reuse_control.name]
            assert len(old) == 1 and old[0]['passed']
            assert old[0]['native_sha256'] == manifest['native_sha256'] and old[0]['executable_sha256'] == sha(executable)
            assert old[0]['ordered_results_sha256'] == sha(args.reuse_control / 'oracle.bin')
            for name in ('oracle.bin', 'stderr.log'): shutil.copyfile(args.reuse_control / name, destination / name)
        else:
            with (destination / 'oracle.bin').open('wb') as out, (destination / 'stderr.log').open('wb') as err:
                subprocess.run(command, stdout=out, stderr=err, timeout=240, check=True)
        data = (destination / 'oracle.bin').read_bytes(); assert data
        reports = [json.loads(line) for line in re.findall(r'^BATCH_RESULTS (.+)$', (destination / 'stderr.log').read_text(), re.M)]
        assert len(reports) == len(oracle) and {(r['classes'], r['variant']) for r in reports} == set(oracle)
        for report in reports:
            key = (report['classes'], report['variant'])
            if report['groups'] != oracle[key]:
                (destination / 'oracle-mismatch.json').write_text(json.dumps({'key': key, 'actual': report['groups'], 'expected': oracle[key]}, indent=2))
                raise SystemExit(f'Independent batch oracle mismatch: {label}, {key}')
        if baseline is None: baseline = data
        assert data == baseline, 'Complete ordered batch bytes differ: ' + label
        records.append({'label': label, 'native_sha256': manifest['native_sha256'], 'executable_sha256': sha(executable),
                        'command': command, 'reused_control': str(args.reuse_control) if index == 0 and args.reuse_control else None,
                        'ordered_results_sha256': hashlib.sha256(data).hexdigest(), 'queries': len(reports), 'passed': True})
        print(json.dumps(records[-1]), flush=True)
    result = {'fixture_sha256': sha(args.fixture / 'strings.apk'), 'independent_rows_sha256': sha(args.fixture / 'oracle-rows.json'),
              'checker_script_sha256': sha(Path(__file__)), 'records': records,
              'coverage': '16 method + 16 class batches; independent raw-row predicates and code-presence parsing; cold/full/repeated/concurrent complete ordered bytes'}
    (args.output / 'validation.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
