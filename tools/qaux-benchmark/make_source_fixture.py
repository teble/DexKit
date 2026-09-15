#!/usr/bin/env python3
"""Build source-file metadata edge cases and optional wide class tables."""

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from make_symbol_fixture import make_dex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--classes-per-dex', type=int, default=0)
    args = parser.parse_args()
    if not 0 <= args.classes_per_dex <= 60000:
        raise SystemExit('Use 0..60000 additional classes per DEX.')
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit('Choose an empty output directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    prefix = 'Lsources/'
    values = {'Absent': None, 'Empty': '', 'Case': 'MiXeD.java',
              'Utf': '\u6e90\u6587\u4ef6.java', 'Mutf': 'A\0B.java',
              'Long': 'prefix-' + 'Long' * 128 + '.java', 'Shared': 'First.java'}
    sources = [{prefix + key + ';': value for key, value in values.items()},
               {prefix + 'Ref;': 'Rare.java'}, {prefix + 'Shared;': 'Last.java'}]
    for dex in (0, 1):
        for i in range(args.classes_per_dex):
            sources[dex][f'{prefix}Bulk{dex}_{i:05d};'] = ('Rare.java' if i == 0 else
                                                         [None, '', 'Shared.java', 'MiXeD.java'][i % 4])
    data = []
    for mapping in sources:
        blob, _ = make_dex({key: () for key in mapping}, set(), set(), source_files=mapping,
                          extras=[prefix + key + ';' for key in [*values, 'Ref', 'Undefined']] + ['I', '[I'])
        data.append(blob)
    apk = args.output / 'sources.apk'
    with zipfile.ZipFile(apk, 'w') as archive:
        for i, blob in enumerate(data, 1):
            info = zipfile.ZipInfo('classes.dex' if i == 1 else f'classes{i}.dex', (2026, 9, 15, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, blob)
    manifest = dict(apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
                    dex_sha256=[hashlib.sha256(blob).hexdigest() for blob in data],
                    classes_per_dex=args.classes_per_dex, defined_rows=sum(map(len, sources)),
                    edge_cases=values, canonical_shared='Last.java',
                    note='No source index and a defined empty string are separate ClassDef cases. '
                         'Strings include UTF-8 and modified-UTF-8 NUL; undefined raw type IDs remain present.')
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(apk)


if __name__ == '__main__':
    main()
