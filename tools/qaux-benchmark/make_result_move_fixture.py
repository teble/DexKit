#!/usr/bin/env python3
"""Bounded method results with nonempty parameter vectors for move comparisons."""

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
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit('Choose a new fixture directory.')
    args.output.mkdir(parents=True)
    owners = ['Lresultmove/C%04d;' % i for i in range(1500)]
    parameters = ('I', 'J', 'Ljava/lang/String;', '[I')
    code = {}
    for i, owner in enumerate(owners):
        for method in range(3):
            code[(owner, 'm%d' % method, 'V', parameters)] = assembler(
                ['Other' if i == 1001 else LONG] * 16)
    data, manifest = make_dex({owner: () for owner in owners}, set(code), set(),
            code=code, additional_strings=[LONG, 'Other'])
    positive = [m for m in manifest['methods'] if not m['descriptor'].startswith(owners[1001] + '->')]
    manifest.update(sha256=hashlib.sha256(data).hexdigest(), parameter_types=parameters,
            parameter_registers=5, expected_positive_methods=len(positive),
            expected_positive_checksum=sum(m['id'] + len(m['descriptor']) for m in positive))
    (args.output / 'classes.dex').write_bytes(data)
    with zipfile.ZipFile(args.output / 'strings.apk', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('classes.dex', data)
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
