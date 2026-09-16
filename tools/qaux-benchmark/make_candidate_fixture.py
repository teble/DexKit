#!/usr/bin/env python3
"""Cross-slice method/interface witnesses and reversed ClassDef enumeration."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import zlib

from make_relation_fixture import assemble
from make_string_fixture import LONG
from make_symbol_fixture import make_dex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit('Choose a new fixture directory.')
    args.output.mkdir(parents=True)
    owner = lambda i: 'Lcandidate/C%04d;' % i
    method = lambda i, j: (owner(i), 'm%d' % j, 'V', ())
    classes = {owner(i): (owner(1000),) if i == 0 else (owner(1001),) if i == 1 else ()
               for i in range(1500)}
    code = {}
    for i in range(1500):
        for j in range(3):
            operations = [('string', 'Other' if i == 1001 else LONG)] * 16
            if i == 0 and j < 2:
                operations.append(('invoke', method(1000 + j, 0)))
            code[method(i, j)] = assemble(operations)
    data, manifest = make_dex(classes, set(code), set(), code=code, additional_strings=[LONG, 'Other'])
    image = bytearray(data)
    count, offset = struct.unpack_from('<II', image, 96)
    definitions = [image[offset + i * 32:offset + (i + 1) * 32] for i in range(count)]
    image[offset:offset + count * 32] = b''.join(reversed(definitions))
    image[12:32] = hashlib.sha1(image[32:]).digest()
    struct.pack_into('<I', image, 8, zlib.adler32(image[12:]) & 0xffffffff)
    data = bytes(image)
    (args.output / 'classes.dex').write_bytes(data)
    with zipfile.ZipFile(args.output / 'strings.apk', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('classes.dex', data)
    manifest.update(sha256=hashlib.sha256(data).hexdigest(), class_def_order='reverse type ID',
                    method_count=4500, class_count=1500,
                    positive_method=dict(caller=0, target=3000), negative_method=dict(caller=1, target=3003),
                    positive_interface=dict(owner=0, target=1000), negative_interface=dict(owner=1, target=1001))
    assert manifest['methods'][3000]['descriptor'] == owner(1000) + '->m0()V'
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
