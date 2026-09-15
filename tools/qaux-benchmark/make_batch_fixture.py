#!/usr/bin/env python3
"""Short repeated strings for isolated batch group-containment workloads."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from make_string_fixture import assembler
from make_symbol_fixture import make_dex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--methods', type=int, default=1500, help='Per DEX; divisible by ten')
    parser.add_argument('--references', type=int, default=4)
    parser.add_argument('--miss', action='store_true')
    args = parser.parse_args()
    assert 10 <= args.methods <= 20000 and args.methods % 10 == 0
    assert 1 <= args.references <= 1024
    assert not args.output.exists() or not any(args.output.iterdir())
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {'methods_per_dex': args.methods, 'references_per_method': args.references,
                'miss': args.miss, 'rows': 'jklmnop' if args.miss else 'abcdefg', 'dexes': []}
    with zipfile.ZipFile(args.output / 'batch.apk', 'w') as archive:
        for dex in range(3):
            rows = {}
            for i in range(args.methods):
                owner = f'Lbatch/Rows{dex}_{i // 10:04d};'
                rows[(owner, f'm{i:05d}', 'V', ())] = [manifest['rows']] * args.references
            classes = {m[0]: [] for m in rows}
            code = {m: assembler(values) for m, values in rows.items()}
            data, info = make_dex(classes, set(rows), set(), code=code, additional_strings={manifest['rows']})
            name = 'classes' + (str(dex + 1) if dex else '') + '.dex'
            entry = zipfile.ZipInfo(name, date_time=(2020, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
            (args.output / name).write_bytes(data)
            info.update(name=name, sha256=hashlib.sha256(data).hexdigest())
            manifest['dexes'].append(info)
    manifest['apk_sha256'] = hashlib.sha256((args.output / 'batch.apk').read_bytes()).hexdigest()
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps({'apk': str(args.output / 'batch.apk'), 'sha256': manifest['apk_sha256']}))


if __name__ == '__main__':
    main()
