#!/usr/bin/env python3
"""Compare a native caller dump against independently authored raw rows."""

import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--dump', type=Path, required=True)
    args = parser.parse_args()
    expected = json.loads(args.manifest.read_text())['expected_raw_callers']
    data = args.dump.read_bytes()
    if data[:8] != b'CALLERS1':
        raise SystemExit('Not a caller dump.')
    offset = 8

    def take(size):
        nonlocal offset
        if size > len(data) - offset:
            raise SystemExit('Truncated caller dump.')
        value = int.from_bytes(data[offset:offset + size], 'little')
        offset += size
        return value

    assert take(4) == len(expected)
    edges = 0
    for dex, methods in enumerate(expected):
        assert take(4) == len(methods), dex
        for method, row in enumerate(methods):
            count = take(8)
            assert count == len(row), (dex, method, count, len(row))
            actual = [[take(2), take(4)] for _ in range(count)]
            assert actual == row, (dex, method, actual, row)
            edges += count
    size = take(8)
    assert size == len(data) - offset
    print(json.dumps(dict(dexes=len(expected), methods=sum(map(len, expected)), edges=edges,
                          full_dump_sha256=hashlib.sha256(data).hexdigest(), independent_rows_equal=True)))


if __name__ == '__main__':
    main()
