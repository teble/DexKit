#!/usr/bin/env python3
"""Unique root posting and a cross-DEX target with the same local method ID."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from make_symbol_fixture import make_dex
from make_relation_fixture import assemble


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert not args.output.exists()
    args.output.mkdir(parents=True)
    caller, target = 'Ladmit/Caller;', 'Ladmit/Target;'
    method = lambda owner, name: (owner, name, 'V', ())
    remote = method(target, 'remote')
    code = {method(caller, 'aPad'): assemble([('string', 'Other')]),
            method(caller, 'call'): assemble([('string', 'OnlyOne')] * 3 + [('invoke', remote)])}
    rows = [make_dex({caller: ()}, set(code), set(), references=[remote], code=code,
                     additional_strings=['OnlyOne', 'Other', 'Second'])]
    code = {method(target, 'aPad'): assemble([('string', 'Second')]),
            remote: assemble([('string', 'Second')])}
    rows.append(make_dex({target: ()}, set(code), set(), code=code,
                         additional_strings=['OnlyOne', 'Other', 'Second']))
    assert rows[0][1]['methods'][1]['descriptor'] == caller + '->call()V'
    assert rows[1][1]['methods'][1]['descriptor'] == target + '->remote()V'
    manifests = []
    with zipfile.ZipFile(args.output / 'strings.apk', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for dex, (data, info) in enumerate(rows):
            name = 'classes.dex' if dex == 0 else 'classes2.dex'
            (args.output / name).write_bytes(data)
            archive.writestr(name, data)
            info.update(name=name, sha256=hashlib.sha256(data).hexdigest())
            manifests.append(info)
    (args.output / 'manifest.json').write_text(json.dumps(dict(dexes=manifests,
        root=dict(dex=0, method=1, word='OnlyOne', instructions=3, distinct_edges=1),
        target=dict(dex=1, method=1, words=['Second'])), indent=2) + '\n')


if __name__ == '__main__':
    main()
