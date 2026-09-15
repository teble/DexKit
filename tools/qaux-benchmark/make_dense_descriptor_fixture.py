#!/usr/bin/env python3
"""Create a high-coverage short-descriptor DEX fixture for cache counterexamples."""

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from make_symbol_fixture import make_dex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--members', type=int, default=60000)
    args = parser.parse_args()
    if not 1 <= args.members <= 65000:
        raise SystemExit('Use 1..65000 members of each kind within one DEX.')
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit('Use an empty fixture directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    owner = 'LA;'
    methods = {(owner, f'm{i:05d}', 'V', ()) for i in range(args.members)}
    fields = {(owner, f'f{i:05d}', 'I') for i in range(args.members)}
    data, _ = make_dex({owner: ()}, methods, fields)
    apk = args.output / 'dense-descriptors.apk'
    with zipfile.ZipFile(apk, 'w') as archive:
        info = zipfile.ZipInfo('classes.dex', (2026, 9, 15, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        archive.writestr(info, data)
    manifest = dict(apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
                    dex_sha256=hashlib.sha256(data).hexdigest(),
                    methods=args.members, fields=args.members, owner=owner,
                    method_pattern='LA;->m%05d()V', field_pattern='LA;->f%05d:I',
                    purpose='Full cache coverage with descriptors short enough for the tested standard library SSO.')
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(apk)


if __name__ == '__main__':
    main()
