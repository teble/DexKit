#!/usr/bin/env python3
"""Build ordered, duplicated invocation rows with dense and sparse methods."""

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from make_symbol_fixture import make_dex
from make_relation_fixture import assemble, method


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--methods', type=int, default=64)
    parser.add_argument('--fanout', type=int, default=1024)
    parser.add_argument('--sparse-every', type=int, default=4)
    args = parser.parse_args()
    if not 1 <= args.methods <= 10000 or not 2 <= args.fanout <= 1000000 or args.sparse_every < 0:
        raise SystemExit('Unsupported bounded fixture size.')
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit('Choose an empty fixture directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    owner = 'Lrelations/Target;'
    early, late = method(owner, 'aEarly'), method(owner, 'zLate')
    value = (owner, 'value', 'I')
    strings = ['invoke-tag', 'caller-tag']
    rows = []
    data, _ = make_dex({owner: ()}, {early, late}, {value},
        code={early: assemble([]), late: assemble([('string', 'invoke-tag')])},
        additional_strings=strings)
    rows.append(data)
    active = 0
    for dex in (1, 2):
        source = f'Lrelations/Source{dex};'
        runs = {method(source, f'run{i:05d}') for i in range(args.methods)}
        last = method(source, 'zRun')
        code = {}
        for i, member in enumerate(sorted(runs)):
            sparse = args.sparse_every != 0 and (i + 1) % args.sparse_every == 0
            operations = [] if sparse else [('get', value), ('string', 'caller-tag')]
            if not sparse:
                operations += [('invoke', early)] * (args.fanout - 1) + [('invoke', late)]
                active += 1
            code[member] = assemble(operations)
        # A distinct caller at the end tests a late witness in reverse rows.
        code[last] = assemble([('get', value), ('string', 'caller-tag'), ('invoke', late)])
        classes, methods, fields = {source: ()}, runs | {last}, set()
        if dex == 2:
            classes[owner] = ()
            methods.update([early, late])
            fields.add(value)
            code[early] = assemble([])
            code[late] = assemble([('string', 'invoke-tag')])
        data, _ = make_dex(classes, methods, fields, references={early, late},
            field_references={value}, code=code, additional_strings=strings,
            extras=(f'Lrelations/Undefined{dex};',))
        rows.append(data)
    apk = args.output / 'invocations.apk'
    with zipfile.ZipFile(apk, 'w') as archive:
        for i, data in enumerate(rows, 1):
            info = zipfile.ZipInfo('classes.dex' if i == 1 else f'classes{i}.dex', (2026, 9, 15, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    manifest = dict(apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
        dex_sha256=[hashlib.sha256(data).hexdigest() for data in rows],
        methods_per_source=args.methods, fanout=args.fanout, sparse_every=args.sparse_every,
        dense_methods=active, forward_edges=active * args.fanout + 2,
        note='Serial loading selects the final Target definition; repeated edges retain positions.')
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(apk)


if __name__ == '__main__':
    main()
