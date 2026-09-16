#!/usr/bin/env python3
"""Generate explicit caller contracts, including an empty DEX and blocked aliases."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import zlib

from make_relation_fixture import assemble, method
from make_symbol_fixture import make_dex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit('Use a new fixture directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    target, blocked = 'Lcaller/Target;', 'Lcaller/Blocked;'
    a, z = method(target, 'aEarly'), method(target, 'zLate')
    missing, present = method(blocked, 'aMissing'), method(blocked, 'zPresent')
    absent = method('Lcaller/Absent;', 'foreign')
    local0, local2 = method(target, 'local0'), method(target, 'local2')
    field = (target, 'value', 'I')
    rows = []

    def add(classes, definitions, operations, references=(), reverse=False):
        code = {member: assemble([('invoke', callee) for callee in calls]) for member, calls in operations.items()}
        data, info = make_dex(classes, definitions, {field} if target in classes else set(),
                              references=references, code=code)
        owners = sorted(classes)
        if reverse:
            data = bytearray(data)
            count, offset = struct.unpack_from('<II', data, 96)
            records = [data[offset + 32 * i:offset + 32 * (i + 1)] for i in range(count)]
            data[offset:offset + 32 * count] = b''.join(reversed(records))
            data[12:32] = hashlib.sha1(data[32:]).digest()
            struct.pack_into('<I', data, 8, zlib.adler32(data[12:]) & 0xffffffff)
            data = bytes(data)
            owners.reverse()
        ids = {entry['descriptor']: entry['id'] for entry in info['methods']}
        desc = lambda m: m[0] + '->' + m[1] + '()V'
        by_symbol = {member: ids[desc(member)] for member in set(definitions) | set(references)}
        local = [[] for _ in info['methods']]
        source = len(rows)
        for owner in owners:
            for member in sorted((m for m in definitions if m[0] == owner), key=by_symbol.__getitem__):
                for callee in operations.get(member, []):
                    local[by_symbol[callee]].append([source, by_symbol[member]])
        rows.append(dict(data=data, info=info, ids=by_symbol, local=local, owners=owners))

    add({target: (), blocked: ()}, {a, z, local0, present},
        {a: [], z: [], local0: [a, a, z], present: [present]})
    for source in [1, 2, 3]:
        owner = f'Lcaller/Source{source};'
        run, last, empty = method(owner, 'run'), method(owner, 'zRun'), method(owner, 'empty')
        definitions = {run, last, empty}
        classes = {owner: ()}
        operations = {run: [a, z, a, missing, present, absent], last: [z, z], empty: []}
        if source == 2:
            classes[target] = ()
            definitions.update([a, z, local2])
            operations.update({a: [], z: [], local2: [a, z, a]})
        add(classes, definitions, operations, {a, z, missing, present, absent}, reverse=source == 2)
    add({'Lcaller/Empty;': ()}, set(), {})

    # The fixture's intended alias contracts are explicit. The earlier Target
    # definition retains its own rows; Source1/3 resolve into the final Target.
    # Blocked.aMissing prevents the old resolver cursor reaching zPresent.
    expected = [[list(row) for row in dex['local']] for dex in rows]
    bindings = []
    for source in [1, 3]:
        for member in [a, z]:
            src, dst = rows[source]['ids'][member], rows[2]['ids'][member]
            expected[2][dst].extend(expected[source][src])
            expected[source][src] = []
            bindings.append([source, src, 2, dst])
    apk = args.output / 'callers.apk'
    with zipfile.ZipFile(apk, 'w') as archive:
        for dex, row in enumerate(rows, 1):
            item = zipfile.ZipInfo('classes.dex' if dex == 1 else f'classes{dex}.dex', (2026, 9, 16, 0, 0, 0))
            item.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(item, row['data'])
    manifest = dict(apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
                    dex_sha256=[hashlib.sha256(row['data']).hexdigest() for row in rows],
                    dexes=[row['info'] for row in rows], class_orders=[row['owners'] for row in rows],
                    explicit_bindings=bindings, expected_raw_callers=expected,
                    note='Oracle comes from authored call operations and explicit aliases, independent of native output.')
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(apk)


if __name__ == '__main__':
    main()
