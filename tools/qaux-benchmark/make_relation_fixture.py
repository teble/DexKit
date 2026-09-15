#!/usr/bin/env python3
"""Create deterministic multi-DEX field/invoke code fixtures without an APK runtime."""

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from make_symbol_fixture import make_dex


def method(owner, name):
    return (owner, name, 'V', ())


def assemble(operations):
    def build(method_ids, field_ids, string_ids):
        units = [0x0012]  # const/4 v0, #0
        for kind, symbol in operations:
            if kind in ('get', 'put'):
                units.extend([0x0060 if kind == 'get' else 0x0067, field_ids[symbol]])
            elif kind == 'invoke':
                units.extend([0x0071, method_ids[symbol], 0])  # invoke-static {}, method
            elif kind == 'string':
                idx = string_ids[symbol]
                if idx > 0xffff:
                    raise ValueError('This fixture requires a non-jumbo const-string index.')
                units.extend([0x001a, idx])
            else:
                raise ValueError(kind)
        return units + [0x000e]  # return-void
    return build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fanout', type=int, default=64)
    parser.add_argument('--methods', type=int, default=8)
    args = parser.parse_args()
    if not 0 <= args.fanout <= 2000000 or not 1 <= args.methods <= 10000:
        raise SystemExit('Fixture dimensions exceed the bounded supported range.')
    args.output.mkdir(parents=True, exist_ok=True)
    target, blocked, absent = 'Lrelations/Target;', 'Lrelations/Blocked;', 'Lrelations/Absent;'
    value, unused = (target, 'value', 'I'), (target, 'unused', 'I')
    blocked_defined = (blocked, 'zLater', 'I')
    blocked_missing = (blocked, 'aMissing', 'I')
    missing = (absent, 'value', 'I')
    leaf = method(target, 'leaf')
    local = method(target, 'local')
    sparse = method(target, 'sparse')
    rows = []
    all_strings = ('field-seed', 'call-seed', 'deep-negative')
    data, manifest = make_dex(
        {target: (), blocked: ()}, {leaf, local, sparse}, {value, unused, blocked_defined},
        code={leaf: assemble([]), sparse: assemble([]),
              local: assemble([('get', value), ('get', value), ('put', value), ('invoke', leaf)])},
        source_files={blocked: None}, additional_strings=all_strings)
    rows.append((data, manifest))
    for dex in (1, 2):
        owner = f'Lrelations/Source{dex};'
        own_field = (owner, 'localValue', 'I')
        methods = {method(owner, f'run{i:04d}') for i in range(args.methods)}
        cold = method(owner, 'cold')
        methods.add(cold)
        operations = [('string', 'field-seed'), ('get', value), ('put', value),
                      ('get', value), ('get', blocked_missing), ('get', blocked_defined),
                      ('put', missing), ('get', own_field)]
        operations += [('invoke', leaf)] * args.fanout
        code = {m: assemble(operations) for m in methods if m != cold}
        code[cold] = assemble([])
        # Source2 repeats Target's declaration. Serial loading makes this last
        # definition the chosen target. Earlier local rows remain independent.
        classes = {owner: ()}
        fields = {own_field}
        if dex == 2:
            classes[target] = ()
            fields.update([value, unused])
            methods.add(leaf)
            code[leaf] = assemble([])
        data, manifest = make_dex(classes, methods, fields, references={leaf},
            field_references={value, unused, blocked_missing, blocked_defined, missing},
            extras=(f'Lrelations/UnusedType{dex};',), code=code,
            additional_strings=all_strings)
        rows.append((data, manifest))
    apk = args.output / 'relations.apk'
    with zipfile.ZipFile(apk, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for number, (data, _) in enumerate(rows, 1):
            name = 'classes.dex' if number == 1 else f'classes{number}.dex'
            info = zipfile.ZipInfo(name, (2026, 9, 15, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    result = dict(apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
                  fanout=args.fanout, methods_per_source=args.methods,
                  dexes=[m for _, m in rows])
    (args.output / 'manifest.json').write_text(json.dumps(result, indent=2) + '\n')
    print(apk)


if __name__ == '__main__':
    main()
