#!/usr/bin/env python3
"""Independent field-row predicates and complete ordered serialized equality."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zipfile

from validate_string_checks import class_ids

QUERY_COUNT = 24


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def decoded_rows(data, info):
    # This deliberately decodes the fixture's small instruction subset without
    # calling either the native field extractor or the generator's assembler.
    def uleb(offset):
        value, shift = 0, 0
        while True:
            byte = data[offset]; offset += 1
            value |= (byte & 127) << shift
            if byte < 128: return value, offset
            shift += 7
            assert shift < 35
    methods = {m['id']: m['descriptor'] for m in info['methods']}
    fields = {f['id']: f['descriptor'] for f in info['fields']}
    count, offset = struct.unpack_from('<2I', data, 96)
    rows = {}
    for index in range(count):
        cursor = struct.unpack_from('<I', data, offset + 32 * index + 24)[0]
        if not cursor: continue
        sizes = []
        for _ in range(4):
            size, cursor = uleb(cursor); sizes.append(size)
        for _ in range(sizes[0] + sizes[1]):
            _, cursor = uleb(cursor); _, cursor = uleb(cursor)
        for size in sizes[2:]:
            method_id = 0
            for _ in range(size):
                delta, cursor = uleb(cursor); method_id += delta
                _, cursor = uleb(cursor); code, cursor = uleb(cursor)
                uses = []
                if code:
                    units = struct.unpack_from('<I', data, code + 12)[0]
                    start, end = code + 16, code + 16 + units * 2
                    while start < end:
                        op = data[start]
                        if op in (0x60, 0x67):
                            field_id = struct.unpack_from('<H', data, start + 2)[0]
                            uses.append(dict(field=fields[field_id], get=op == 0x60)); start += 4
                        else:
                            assert op in (0x12, 0x0e)
                            start += 2
                    assert start == end
                rows[methods[method_id]] = (bool(code), uses)
    return rows


def expected(fixture):
    manifest = json.loads((fixture / 'manifest.json').read_text())
    assert manifest['apk_sha256'] == sha(fixture / 'fields.apk')
    rows = manifest['rows']
    definitions, local_ids = {}, {}
    for dex, info in enumerate(manifest['dexes']):
        for field in info['fields']:
            symbol = field['descriptor']
            local_ids[(dex, symbol)] = field['id']
            if field['defined']:
                # This oracle covers unique definitions. Duplicate-definition
                # representative and cursor behavior use the relation checker.
                assert symbol not in definitions
                definitions[symbol] = (dex, field['id'])
    def identity(dex, symbol):
        return definitions.get(symbol, (dex, local_ids[(dex, symbol)]))
    readers, writers = set(), set()
    for row in rows:
        for use in row['uses']:
            (readers if use['get'] else writers).add(identity(row['dex'], use['field']))

    def matches(row, variant):
        uses = row['uses']
        alpha = [u for u in uses if u['field'].endswith('->alpha:I')]
        beta = [u for u in uses if u['field'].endswith('->beta:I')]
        if variant in (0, 1): return True
        if variant in (2, 14): return bool(alpha)
        if variant == 3: return any(u['get'] for u in alpha)
        if variant == 4: return any(not u['get'] for u in alpha)
        if variant in (5, 6, 7): return bool(uses)
        if variant == 8: return len(alpha) >= 2
        if variant == 9: return any(u['get'] for u in alpha) and any(not u['get'] for u in alpha)
        if variant == 10: return bool(alpha and beta)
        if variant in (11, 21): return False
        if variant == 12: return bool(beta)
        if variant == 13: return any(u['field'] == 'Lufields/Absent;->alpha:I' for u in alpha)
        if variant == 15: return any(identity(row['dex'], u['field']) in readers for u in alpha)
        if variant == 16: return any(identity(row['dex'], u['field']) in writers for u in alpha)
        if variant == 17: return not alpha
        if variant == 18: return bool(alpha or beta)
        if variant == 19: return any(u['get'] for u in alpha)
        if variant == 20: return row['descriptor'].endswith('->unique()V') and bool(alpha)
        if variant == 22: return len(uses) >= 2
        if variant == 23: return row['descriptor'].endswith('->early00000()V') and bool(alpha)
        raise AssertionError(variant)

    methods = {(dex, m['descriptor']): m['id'] for dex, info in enumerate(manifest['dexes']) for m in info['methods']}
    with zipfile.ZipFile(fixture / 'fields.apk') as archive:
        classes = {}
        for dex, info in enumerate(manifest['dexes']):
            data = archive.read('classes.dex' if dex == 0 else f'classes{dex + 1}.dex')
            actual_rows = decoded_rows(data, info)
            wanted_rows = {r['descriptor']: (r['code'], r['uses']) for r in rows if r['dex'] == dex}
            assert actual_rows == wanted_rows, 'Encoded field-use rows differ from the intended fixture'
            classes[dex] = class_ids(data)
    answers = {}
    for is_class in (False, True):
        for variant in range(QUERY_COUNT):
            matching = [r for r in rows if matches(r, variant)]
            if is_class:
                owners = {(r['dex'], r['owner']) for r in matching}
                result = [[dex, index, owner] for dex, items in classes.items() for index, owner in items
                          if (dex, owner) in owners]
            else:
                result = [[r['dex'], methods[(r['dex'], r['descriptor'])], r['descriptor']] for r in matching]
                result.sort(key=lambda item: (item[0], item[1]))
            if variant == 20: assert len(result) == 1
            answers[(is_class, variant)] = result
    return answers


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--variant', nargs=2, action='append', required=True)
    args = parser.parse_args()
    assert not args.output.exists() or not any(args.output.iterdir())
    args.output.mkdir(parents=True, exist_ok=True)
    oracle, baseline, records = expected(args.fixture), None, []
    for label, directory in args.variant:
        artifact = Path(directory)
        manifest = json.loads((artifact / 'artifact.json').read_text())
        assert sha(artifact / 'libdexkit.dylib') == manifest['native_sha256']
        executable = artifact / 'build/Core/dexkit_field_checks'
        destination = args.output / label; destination.mkdir()
        command = [str(executable), '--dump', str(args.fixture / 'fields.apk')]
        with (destination / 'oracle.bin').open('wb') as out, (destination / 'stderr.log').open('wb') as err:
            subprocess.run(command, stdout=out, stderr=err, timeout=300, check=True)
        reports = [json.loads(s) for s in re.findall(r'^FIELD_RESULTS (.+)$', (destination / 'stderr.log').read_text(), re.M)]
        assert len(reports) == len(oracle) and {(r['classes'], r['variant']) for r in reports} == set(oracle)
        for report in reports:
            key = (report['classes'], report['variant'])
            if report['results'] != oracle[key]:
                (destination / 'mismatch.json').write_text(json.dumps(dict(key=key, actual=report['results'], expected=oracle[key]), indent=2))
                raise SystemExit(f'Independent field oracle differs: {label}, {key}')
        data = (destination / 'oracle.bin').read_bytes(); assert data
        if baseline is None: baseline = data
        assert data == baseline, 'Complete ordered field results differ: ' + label
        record = dict(label=label, command=command, native_sha256=manifest['native_sha256'],
                      executable_sha256=sha(executable), ordered_results_sha256=sha(destination / 'oracle.bin'),
                      queries=len(reports), passed=True)
        records.append(record); print(json.dumps(record), flush=True)
    result = dict(fixture_sha256=sha(args.fixture / 'fields.apk'), manifest_sha256=sha(args.fixture / 'manifest.json'),
                  checker_script_sha256=sha(Path(__file__)), records=records,
                  coverage='24 method and 24 nested class queries; independent field-row predicates; cold/full/repeat/concurrent complete ordered bytes')
    (args.output / 'validation.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
