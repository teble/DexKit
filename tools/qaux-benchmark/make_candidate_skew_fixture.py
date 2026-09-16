#!/usr/bin/env python3
"""Bounded mixed-size DEX files for preparation-window scheduling checks."""

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from make_string_fixture import LONG, assembler
from make_symbol_fixture import make_dex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dexes', type=int, default=16)
    parser.add_argument('--heavy-methods', type=int, default=10000)
    parser.add_argument('--light-methods', type=int, default=128)
    parser.add_argument('--heavy-at', type=int, default=0)
    args = parser.parse_args()
    if not 2 <= args.dexes <= 32 or not 1 <= args.heavy_methods <= 20000 or not 1 <= args.light_methods <= 1000:
        raise SystemExit('Fixture dimensions exceed the bounded supported range.')
    if not 0 <= args.heavy_at < args.dexes or args.output.exists():
        raise SystemExit('Choose a valid heavy DEX and a new output directory.')
    args.output.mkdir(parents=True)
    manifest = dict(dexes=[], expected_positive_methods=0, expected_positive_classes=args.dexes)
    with zipfile.ZipFile(args.output/'strings.apk', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for dex in range(args.dexes):
            owner = 'Lskew/Dex%02d;' % dex
            count = args.heavy_methods if dex == args.heavy_at else args.light_methods
            rows = {(owner, 'm%05d' % i, 'V', ()): [LONG + '/%02d/%05d' % (dex, i)] * 16 for i in range(count)}
            words = {row[0] for row in rows.values()}
            data, info = make_dex({owner: ()}, set(rows), set(), code={method: assembler(values) for method, values in rows.items()},
                                  additional_strings=words)
            name = 'classes.dex' if dex == 0 else 'classes%d.dex' % (dex + 1)
            (args.output/name).write_bytes(data)
            archive.writestr(name, data)
            manifest['dexes'].append(dict(name=name, methods=count, sha256=hashlib.sha256(data).hexdigest()))
            manifest['expected_positive_methods'] += count
    (args.output/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')


if __name__ == '__main__':
    main()
