#!/usr/bin/env python3
"""Small cross-DEX string/TLS fixture and one-DEX broad-candidate guard."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from make_symbol_fixture import make_dex
from make_relation_fixture import assemble
from make_string_fixture import LONG


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--broad', action='store_true')
    args = parser.parse_args()
    assert not args.output.exists()
    args.output.mkdir(parents=True)
    method = lambda owner, name: (owner, name, 'V', ())
    rows = []
    if args.broad:
        owner = 'Lsinv/Broad;'
        methods = {method(owner, 'm%05d' % i) for i in range(4500)}
        code = {m: assemble([('string', LONG)] * 16) for m in sorted(methods)}
        rows.append(make_dex({owner: ()}, methods, set(), code=code, additional_strings=[LONG]))
    else:
        caller, only, target = 'Lsinv/Caller;', 'Lsinv/Only;', 'Lsinv/Target;'
        remote = method(target, 'remote')
        code = {
            method(caller, 'both'): assemble([('string', 'Needle'), ('string', 'Second')]),
            method(caller, 'call'): assemble([('string', 'Needle'), ('invoke', remote)]),
            method(caller, 'only'): assemble([('string', 'Needle')]),
            method(only, 'r'): assemble([('string', 'Needle')]),
        }
        rows.append(make_dex({caller: (), only: ()}, set(code), set(), references=[remote],
                             code=code, additional_strings=['Needle', 'Second']))
        code = {method(target, 'aPad'): assemble([('string', 'Second')]),
                remote: assemble([('string', 'Second')])}
        rows.append(make_dex({target: ()}, set(code), set(), code=code, additional_strings=['Needle', 'Second']))
        # Deliberately collide the root caller ID with its remote target ID.
        assert rows[0][1]['methods'][1]['descriptor'] == caller + '->call()V'
        assert rows[1][1]['methods'][1]['descriptor'] == target + '->remote()V'
    manifests = []
    with zipfile.ZipFile(args.output / 'strings.apk', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for i, (data, manifest) in enumerate(rows):
            name = 'classes' + (str(i + 1) if i else '') + '.dex'
            archive.writestr(name, data)
            (args.output / name).write_bytes(data)
            manifest.update(name=name, sha256=hashlib.sha256(data).hexdigest())
            manifests.append(manifest)
    (args.output / 'manifest.json').write_text(json.dumps({'broad': args.broad, 'dexes': manifests}, indent=2) + '\n')


if __name__ == '__main__':
    main()
