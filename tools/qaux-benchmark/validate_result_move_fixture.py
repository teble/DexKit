#!/usr/bin/env python3
"""Check complete result bytes and parameter metadata against the input DEX."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def field(data, table, index):
    vtable = table - struct.unpack_from('<i', data, table)[0]
    if 4 + 2 * index >= struct.unpack_from('<H', data, vtable)[0]:
        return None
    delta = struct.unpack_from('<H', data, vtable + 4 + 2 * index)[0]
    return table + delta if delta else None


def indirect(data, offset):
    return offset + u32(data, offset)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--variant', nargs=2, action='append', required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True)
    manifest = json.loads((args.fixture / 'manifest.json').read_text())
    dex = (args.fixture / 'classes.dex').read_bytes()
    assert hashlib.sha256(dex).hexdigest() == manifest['sha256']
    type_names = []
    for type_id in range(u32(dex, 64)):
        string_id = u32(dex, u32(dex, 68) + 4 * type_id)
        offset = u32(dex, u32(dex, 60) + 4 * string_id)
        while dex[offset] & 128:
            offset += 1
        offset += 1
        type_names.append(dex[offset:dex.index(b'\0', offset)].decode('utf-8'))
    records, payloads = [], []
    for label, artifact in args.variant:
        executable = Path(artifact) / 'build/Core/dexkit_string_checks'
        destination = args.output / (label + '.bin')
        with destination.open('wb') as out, (args.output / (label + '.log')).open('w') as err:
            subprocess.run([str(executable), '--dump', str(args.fixture / 'strings.apk')],
                    stdout=out, stderr=err, check=True, timeout=120)
        raw = destination.read_bytes()
        payloads.append(raw)
        offset, case, checked, seen = 0, 0, 0, set()
        while offset < len(raw):
            size = u32(raw, offset)
            data = raw[offset + 4:offset + 4 + size]
            offset += 4 + size
            # string_checks emits 45 method queries, followed by 45 class queries.
            if case < 45:
                vector = indirect(data, field(data, u32(data, 0), 0))
                for i in range(u32(data, vector)):
                    method = indirect(data, vector + 4 + 4 * i)
                    id_at = field(data, method, 0)
                    identity = u32(data, id_at) if id_at is not None else 0
                    parameters = indirect(data, field(data, method, 6))
                    values = [type_names[u32(data, parameters + 4 + 4 * j)]
                            for j in range(u32(data, parameters))]
                    assert values == manifest['parameter_types'], (case, identity, values)
                    descriptor = indirect(data, field(data, method, 4))
                    value = data[descriptor + 4:descriptor + 4 + u32(data, descriptor)].decode()
                    assert value == manifest['methods'][identity]['descriptor']
                    seen.add(identity)
                    checked += 1
            case += 1
        # Match-all cases cover all methods, including the three without Needle.
        assert offset == len(raw) and case == 90 and len(seen) == len(manifest['methods'])
        records.append(dict(label=label, checked_method_records=checked, distinct_methods=len(seen),
                ordered_payload_sha256=hashlib.sha256(raw).hexdigest(),
                executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest()))
        print(label, checked, len(seen), flush=True)
    assert all(payload == payloads[0] for payload in payloads)
    (args.output / 'validation.json').write_text(json.dumps(records, indent=2) + '\n')


if __name__ == '__main__':
    main()
