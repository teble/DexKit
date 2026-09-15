#!/usr/bin/env python3
"""Named and unnamed string candidates for conservative route admission."""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import zipfile

from make_string_fixture import LONG, assembler, raw_mutf8
from make_symbol_fixture import make_dex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--per-group', type=int, default=2250)
    parser.add_argument('--dexes', type=int, choices=(1, 2), default=1)
    args = parser.parse_args()
    if not 2 <= args.per_group <= 10000:
        raise SystemExit('Choose between 2 and 10000 methods per group.')
    if args.output.exists():
        raise SystemExit('Choose a new output directory.')
    args.output.mkdir(parents=True)

    rows = {}
    classes = {'Ladmission/Rows;': (), 'Ladmission/Special;': ()}
    for i in range(args.per_group):
        rows[('Ladmission/Rows;', 'm%05d' % i, 'V', ())] = [LONG + '/unique/%05d' % i] * 16
        owner = 'Ladmission/Wide%05d;' % i
        classes[owner] = ()
        rows[(owner, 'sharedName', 'V', ())] = [LONG + '/wide/%05d' % i] * 16
    rows[('Ladmission/Rows;', 'm00000', 'V', ())].append('OneLargeClass')
    rows[('Ladmission/Special;', 'single', 'V', ())] = [
        'OnlyOne', 'OnlyOne', 'OnlyOne', 'SinglePrefix/one', 'SameMethodPrefix/a', 'SameMethodPrefix/b', LONG,
    ]
    rows[('Ladmission/Special;', 'pairA', 'V', ())] = ['ExactlyTwo', 'PairPrefix/a', LONG]
    rows[('Ladmission/Special;', 'pairB', 'V', ())] = ['ExactlyTwo', 'PairPrefix/b', LONG]
    rows[('Ladmission/Special;', 'emptyCode', 'V', ())] = []
    rows[('Ladmission/Special;', 'noCode', 'V', ())] = None

    strings = {value for values in rows.values() if values for value in values}
    strings.add('PresentButUnreferenced')
    manifest = dict(per_group=args.per_group, dexes=[])
    oracle = []
    with zipfile.ZipFile(args.output / 'strings.apk', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for dex in range(args.dexes):
            owner = lambda value: value.replace('Ladmission/', 'Ladmission2/') if dex else value
            members = {(owner(key[0]), *key[1:]): values for key, values in rows.items()}
            data, info = make_dex({owner(key): value for key, value in classes.items()}, set(members), set(),
                code={method: assembler(values) for method, values in members.items() if values is not None},
                additional_strings=strings)
            by_descriptor = {method[0] + '->' + method[1] + '()V': values for method, values in members.items()}
            for entry in info['methods']:
                values = by_descriptor[entry['descriptor']]
                oracle.append({**entry, 'dex': dex, 'code': values is not None,
                    'strings_b64': [base64.b64encode(raw_mutf8(value)).decode('ascii') for value in values or []]})
            name = 'classes.dex' if dex == 0 else 'classes2.dex'
            info.update(name=name, sha256=hashlib.sha256(data).hexdigest())
            manifest['dexes'].append(info)
            (args.output / name).write_bytes(data)
            archive.writestr(name, data)
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (args.output / 'oracle-rows.json').write_text(json.dumps(oracle, indent=2) + '\n')


if __name__ == '__main__':
    main()
