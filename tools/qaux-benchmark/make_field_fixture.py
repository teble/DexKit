#!/usr/bin/env python3
"""Deterministic field-use rows with separate early, late, miss and empty cases."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from make_relation_fixture import assemble, method
from make_symbol_fixture import make_dex


def descriptor(symbol):
    return symbol[0] + '->' + symbol[1] + ':' + symbol[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--methods', type=int, default=8)
    parser.add_argument('--row-size', type=int, default=8)
    parser.add_argument('--split-unresolved', action='store_true')
    args = parser.parse_args()
    if not 1 <= args.methods <= 5000 or not 2 <= args.row_size <= 65536:
        raise SystemExit('Use 1..5000 methods per row family and 2..65536 field uses.')
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit('Choose an empty fixture output directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    target = 'Lufields/Target;'
    alpha, beta, noise = [(target, name, 'I') for name in ('alpha', 'beta', 'noise')]
    absent = ('Lufields/Absent;', 'alpha', 'I')
    all_fields = {alpha, beta, noise, absent}
    rows = []
    encoded = []

    def add_dex(classes, fields, patterns):
        methods = {method(owner, name) for owner, name, _ in patterns}
        code = {method(owner, name): assemble(ops) for owner, name, ops in patterns if ops is not None}
        data, manifest = make_dex(classes, methods, fields, field_references=all_fields,
                                  code=code)
        dex = len(encoded)
        for owner, name, ops in patterns:
            rows.append(dict(dex=dex, descriptor=owner + '->' + name + '()V', owner=owner,
                             code=ops is not None,
                             uses=[dict(field=descriptor(symbol), get=kind == 'get')
                                   for kind, symbol in (ops or [])]))
        encoded.append((data, manifest))

    add_dex({target: ()}, {alpha, beta, noise}, [
        (target, 'local', [('get', alpha), ('put', beta)]), (target, 'noCode', None)])
    for dex in (1, 2):
        owner = f'Lufields/Cases{dex};'
        definitions = {
            'empty': [], 'noCode': None,
            'getOnly': [('get', alpha)], 'putOnly': [('put', alpha)],
            'duplicate': [('get', alpha), ('get', alpha)],
            'mixed': [('get', alpha), ('put', alpha)],
            'oneBeta': [('get', beta)], 'putBeta': [('put', beta)],
            'both': [('get', alpha), ('put', beta)],
            'swap': [('put', beta), ('get', alpha)],
            'missing': [('put', absent)], 'noiseOnly': [('get', noise)],
        }
        if args.split_unresolved and dex == 2:
            definitions['missing'] = [('get', absent)]
        if dex == 1: definitions['unique'] = [('get', alpha)]
        for index in range(args.methods):
            tail = [('get' if i % 2 else 'put', noise) for i in range(args.row_size - 2)]
            definitions[f'early{index:05d}'] = [('get', alpha), *tail, ('put', beta)]
            definitions[f'late{index:05d}'] = [*tail, ('get', alpha), ('put', beta)]
            definitions[f'miss{index:05d}'] = [('get' if i % 2 else 'put', noise) for i in range(args.row_size)]
        add_dex({owner: ()}, set(), [(owner, name, definitions[name]) for name in sorted(definitions)])
    apk = args.output / 'fields.apk'
    with zipfile.ZipFile(apk, 'w') as archive:
        for index, (data, _) in enumerate(encoded, 1):
            name = 'classes.dex' if index == 1 else f'classes{index}.dex'
            info = zipfile.ZipInfo(name, (2026, 9, 15, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    manifest = dict(apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(), methods=args.methods,
                    row_size=args.row_size, split_unresolved=args.split_unresolved,
                    rows=rows, dexes=[item for _, item in encoded])
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(apk)


if __name__ == '__main__':
    main()
