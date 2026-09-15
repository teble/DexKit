#!/usr/bin/env python3
"""Build sorted MUTF-8 string pools and ordered const-string reference rows."""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import zipfile

from make_symbol_fixture import make_dex, mutf8


LONG = 'LongPrefix/' + 'x' * 192 + '/Needle'


def raw_mutf8(value):
    data = mutf8(value)
    offset = 0
    while data[offset] & 128:
        offset += 1
    return bytes(data[offset + 1:-1])


def assembler(values):
    def emit(method_ids, field_ids, string_ids):
        units = []
        for position, value in enumerate(values):
            index = string_ids[value]
            # Exercise both encodings, including valid jumbo uses of small IDs.
            units += [0x1b, index & 0xffff, index >> 16] if index > 0xffff or position % 3 == 0 else [0x1a, index]
        return units + [0x0e]
    return emit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--bulk-methods', type=int, default=0, help='Per DEX, at most 20000')
    parser.add_argument('--references', type=int, default=32)
    parser.add_argument('--padding-strings', type=int, default=0)
    parser.add_argument('--layout', choices=['early', 'late', 'miss', 'dense'], default='late')
    parser.add_argument('--prefix-hit-suffix', action='store_true', help='Use distinct proper extensions of LONG as bulk witnesses')
    args = parser.parse_args()
    if not 0 <= args.bulk_methods <= 20000 or not 1 <= args.references <= 1024:
        raise SystemExit('Fixture dimensions out of range.')
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit('Choose an empty output directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {'scope': 'DEX parser/query fixture, not executable Android code.',
                'bulk_methods_per_dex': args.bulk_methods, 'references': args.references,
                'layout': args.layout, 'dexes': []}
    positions = {'early': [0], 'late': [args.references - 1], 'miss': [],
                 'dense': list(range(args.references))}[args.layout]
    manifest['bulk_long_witness_positions'] = positions
    manifest['bulk_prefix_hit_suffix'] = args.prefix_hit_suffix
    oracle = []
    with zipfile.ZipFile(args.output / 'strings.apk', 'w') as archive:
        for dex in range(3):
            owner = f'Lstrings/Rows{dex};'
            rows = {
                (owner, 'emptyCode', 'V', ()): [],
                (owner, 'noCode', 'V', ()): None,
                (owner, 'equalFirst', 'V', ()): ['Needle', 'filler-a', 'filler-b'],
                (owner, 'equalLast', 'V', ()): ['filler-a', 'filler-b', 'Needle'],
                (owner, 'duplicates', 'V', ()): ['Needle', 'Needle', 'Needle'],
                (owner, 'prefix', 'V', ()): ['Needle-tail'],
                (owner, 'interior', 'V', ()): ['xNeedle-tail'],
                (owner, 'suffix', 'V', ()): ['xNeedle'],
                (owner, 'lowercase', 'V', ()): ['needle'],
                (owner, 'emptyValue', 'V', ()): [''],
                (owner, 'nulLeading', 'V', ()): ['\0Needle'],
                (owner, 'nulSuffix', 'V', ()): ['Needle\0tail'],
                (owner, 'unicode', 'V', ()): ['\u03bbNeedle', 'Needle\u03bb', '\u6e90\u6587\u4ef6'],
                (owner, 'surrogates', 'V', ()): ['\ud800Needle', 'Needle\ud800', '\udc00', '\U0001f600', '\ud800'],
                (owner, 'del', 'V', ()): ['prefix\x7f-tail', 'prefix\x7f', 'prefix\u0080', '\x7fNeedle', '\x01Needle'],
                (owner, 'short', 'V', ()): ['Nee'],
                (owner, 'two', 'V', ()): ['Needle', 'Second'],
                (owner, 'long', 'V', ()): [LONG, LONG + '-tail', 'x' + LONG],
                ('Lstrings/Shared;', 'same', 'V', ()): [['Needle'], ['Other'], ['Needle-tail']][dex],
                (f'Lstrings/Distributed{dex};', 'a', 'V', ()): ['Needle'],
                (f'Lstrings/Distributed{dex};', 'b', 'V', ()): ['Second'],
            }
            if dex == 1:
                rows[('Lstrings/OnlySecond;', 'only', 'V', ())] = ['OnlySecond', 'Needle']
            if dex == 0:
                rows[('Lstrings/Sparse;', 'only', 'V', ())] = ['Needle', LONG]
            for i in range(args.bulk_methods):
                # The last byte must never recreate LONG's lowercase 'e'.
                filler = LONG[:-1] + chr(ord('A') + (i % 20))
                hit = LONG + f'-tail-{dex}-{i:05d}' if args.prefix_hit_suffix else LONG
                values = [filler] * args.references
                if args.layout == 'early': values[0] = hit
                elif args.layout == 'late': values[-1] = hit
                elif args.layout == 'dense': values = [hit] * args.references
                assert [pos for pos, value in enumerate(values)
                        if (value.startswith(LONG) if args.prefix_hit_suffix else value == LONG)] == positions
                rows[(f'Lstrings/Bulk{dex};', f'm{i:05d}', 'V', ())] = values
            classes = {m[0]: [] for m in rows}
            classes[f'Lstrings/NoMethods{dex};'] = []
            extra_strings = {'\0', 'A', 'B', 'Z', '\uffff', '\ud800', '\udfff', 'UnusedNeedle'}
            extra_strings.update(f'000-padding-{dex}-{i:06d}' for i in range(args.padding_strings + dex * 17))
            extra_strings.update(value for values in rows.values() if values is not None for value in values)
            code = {method: assembler(values) for method, values in sorted(rows.items()) if values is not None}
            data, info = make_dex(classes, set(rows), set(), code=code, additional_strings=extra_strings)
            name = 'classes' + (str(dex + 1) if dex else '') + '.dex'
            entry = zipfile.ZipInfo(name, date_time=(2020, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
            (args.output / name).write_bytes(data)
            info.update(name=name, sha256=hashlib.sha256(data).hexdigest())
            manifest['dexes'].append(info)
            row_data = {}
            for method, values in rows.items():
                owner_name, method_name, ret, params = method
                descriptor = owner_name + '->' + method_name + '(' + ''.join(params) + ')' + ret
                row_data[descriptor] = [] if values is None else [base64.b64encode(raw_mutf8(v)).decode() for v in values]
            oracle.append({'dex': dex, 'classes': sorted(classes), 'method_rows': row_data})
    manifest['apk_sha256'] = hashlib.sha256((args.output / 'strings.apk').read_bytes()).hexdigest()
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=True) + '\n')
    # Only the small correctness fixture needs the complete independent rows.
    if args.bulk_methods == 0:
        (args.output / 'oracle-rows.json').write_text(json.dumps(oracle, indent=2, ensure_ascii=True) + '\n')
    print(json.dumps({'apk': str(args.output / 'strings.apk'), 'sha256': manifest['apk_sha256'],
                      'methods': sum(len(d['methods']) for d in manifest['dexes'])}), flush=True)


if __name__ == '__main__':
    main()
