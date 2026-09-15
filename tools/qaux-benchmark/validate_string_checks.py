#!/usr/bin/env python3
"""Compare native string results to independent fixture rows and ordered bytes."""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

LONG = b'LongPrefix/' + b'x' * 192 + b'/Needle'
QUERY_COUNT = 45
FOLD = bytes.maketrans(bytes(range(65, 91)), bytes(range(97, 123)))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def predicate(rows, variant):
    if variant in (0, 9): return True
    if variant == 8: return bool(rows)
    if variant == 22: return predicate(rows, 19) or predicate(rows, 17)
    if variant == 23: return not predicate(rows, 1) and predicate(rows, 3)
    if variant == 24: return predicate(rows, 1) and (predicate(rows, 19) or predicate(rows, 20))
    if variant == 25: return predicate(rows, 1)
    if variant == 35: return predicate(rows, 2) and any(row.startswith(b'Need') for row in rows)
    if variant == 37: return predicate(rows, 36) or predicate(rows, 42)
    if variant == 38: return not predicate(rows, 2) and predicate(rows, 3)
    values = {
        1: b'Needle', 2: b'Needle', 3: b'Needle', 4: b'Needle', 5: b'needle', 6: b'needle',
        7: b'', 10: b'\xce\xbbNeedle', 11: b'\xef\xbf\xbf', 12: b'\xed\xa0\x80',
        13: b'\xf0\x9f\x98\x80', 14: b'\xed\xa0\xbd\xed\xb8\x80', 15: b'prefix\x7f',
        16: LONG, 17: b'OnlySecond', 18: b'UnusedNeedle', 19: b'AbsentEveryPool',
        26: b'Needle', 27: b'OnlySecond', 28: b'Needle', 29: b'\x7fNeedle',
        30: b'Needle\0tail', 31: b'Needle\xc0\x80tail', 32: b'Needle', 33: b'Needle',
        34: LONG, 36: b'AbsentEveryPool', 39: b'\xce\xbbNeedle', 40: b'\x7f',
        41: b'Needle', 42: b'OnlySecond', 43: b'Needle\0', 44: b'Needle\xc0\x80',
    }
    if variant == 20: return b'Needle' in rows and b'Second' in rows
    if variant == 21: return any(b'Needle' in row for row in rows) and b'Second' in rows
    value = values[variant]
    if variant in (5, 32):
        value = value.translate(FOLD)
        rows = [row.translate(FOLD) for row in rows]
    if variant in (2, 15, 32, 34, 36, 39, 40, 41, 42, 43, 44): return any(row.startswith(value) for row in rows)
    if variant == 3: return any(value in row for row in rows)
    if variant == 4: return any(row.endswith(value) for row in rows)
    return value in rows


def class_ids(data):
    string_count, string_off, type_count, type_off = struct.unpack_from('<4I', data, 56)
    count, offset = struct.unpack_from('<2I', data, 96)
    result = []
    for i in range(count):
        type_id = struct.unpack_from('<I', data, offset + 32 * i)[0]
        assert type_id < type_count
        string_id = struct.unpack_from('<I', data, type_off + 4 * type_id)[0]
        assert string_id < string_count
        start = struct.unpack_from('<I', data, string_off + 4 * string_id)[0]
        while data[start] & 128: start += 1
        start += 1
        name = data[start:data.index(b'\0', start)].decode('ascii')
        result.append((type_id, name))
    return result


def expected(fixture):
    manifest = json.loads((fixture / 'manifest.json').read_text())
    independent = json.loads((fixture / 'oracle-rows.json').read_text())
    answer = {}
    for classes in (False, True):
        for variant in range(QUERY_COUNT):
            result, seen = [], set()
            for dex, (info, record) in enumerate(zip(manifest['dexes'], independent)):
                rows = {key: [base64.b64decode(v) for v in values] for key, values in record['method_rows'].items()}
                by_class = {name: [] for name in record['classes']}
                for descriptor, values in rows.items(): by_class[descriptor.split('->')[0]].extend(values)
                items = class_ids((fixture / info['name']).read_bytes()) if classes else [
                    (m['id'], m['descriptor']) for m in info['methods'] if m['defined']]
                for identity, descriptor in items:
                    owner = descriptor if classes else descriptor.split('->')[0]
                    if variant == 26 and owner != 'Lstrings/Sparse;': continue
                    values = by_class[owner] if classes or variant == 25 else rows[descriptor]
                    # FindMethod keeps the first descriptor representative;
                    # FindClass retains matching duplicate DEX definitions.
                    if predicate(values, variant) and (classes or descriptor not in seen):
                        seen.add(descriptor)
                        result.append([dex, identity, descriptor])
            if variant in (27, 42):
                assert len(result) == 1
            answer[(classes, variant)] = result
    return answer


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--variant', nargs=2, action='append', required=True, metavar=('LABEL', 'ARTIFACT'))
    parser.add_argument('--reuse-control', type=Path, help='Completed first-variant run with oracle.bin and stderr.log')
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()): raise SystemExit('Choose a new output directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    oracle = expected(args.fixture)
    baseline = None
    records = []
    for index, (label, artifact) in enumerate(args.variant):
        artifact = Path(artifact)
        executable = artifact / 'build/Core/dexkit_string_checks'
        manifest = json.loads((artifact / 'artifact.json').read_text())
        assert sha(artifact / 'libdexkit.dylib') == manifest['native_sha256']
        destination = args.output / label
        destination.mkdir()
        command = [str(executable), '--dump', str(args.fixture / 'strings.apk')]
        if index == 0 and args.reuse_control:
            prior = json.loads((args.reuse_control.parent / 'validation.json').read_text())
            assert prior['fixture_sha256'] == sha(args.fixture / 'strings.apk'), 'Reused fixture differs'
            matches = [r for r in prior['records'] if r['label'] == args.reuse_control.name]
            assert len(matches) == 1 and matches[0]['passed']
            old = matches[0]
            assert old['native_sha256'] == manifest['native_sha256'], 'Reused native artifact differs'
            assert old['executable_sha256'] == sha(executable), 'Reused checker executable differs'
            assert old['ordered_results_sha256'] == sha(args.reuse_control / 'oracle.bin'), 'Reused output changed'
            for name in ('oracle.bin', 'stderr.log'): shutil.copyfile(args.reuse_control / name, destination / name)
        else:
            with (destination / 'oracle.bin').open('wb') as out, (destination / 'stderr.log').open('wb') as err:
                subprocess.run(command, stdout=out, stderr=err, timeout=240, check=True)
        data = (destination / 'oracle.bin').read_bytes()
        assert data, 'Incomplete native check output'
        reports = [json.loads(line) for line in re.findall(r'^STRING_RESULTS (.+)$', (destination / 'stderr.log').read_text(), re.M)]
        assert len(reports) == len(oracle)
        assert {(r['classes'], r['variant']) for r in reports} == set(oracle), 'Missing or duplicate query reports'
        for report in reports:
            key = (report['classes'], report['variant'])
            if report['results'] != oracle[key]:
                (destination / 'oracle-mismatch.json').write_text(json.dumps({'key': key, 'actual': report['results'], 'expected': oracle[key]}, indent=2))
                raise SystemExit(f'Independent row oracle mismatch: {label}, {key}')
        if baseline is None: baseline = data
        assert data == baseline, 'Ordered serialized results differ: ' + label
        records.append({'label': label, 'native_sha256': manifest['native_sha256'], 'executable_sha256': sha(executable),
                        'command': command, 'reused_control': str(args.reuse_control) if index == 0 and args.reuse_control else None,
                        'ordered_results_sha256': hashlib.sha256(data).hexdigest(), 'queries': len(reports), 'passed': True})
        print(json.dumps(records[-1]), flush=True)
    result = {'fixture_sha256': sha(args.fixture / 'strings.apk'), 'independent_rows_sha256': sha(args.fixture / 'oracle-rows.json'),
              'checker_script_sha256': sha(Path(__file__)), 'records': records,
              'coverage': '45 method + 45 class queries, all UTF-16 units in range oracle, cold/full/repeated/concurrent sequences, ordered serialized equality'}
    (args.output / 'validation.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
