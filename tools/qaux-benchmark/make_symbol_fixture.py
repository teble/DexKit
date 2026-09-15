#!/usr/bin/env python3
"""Build small deterministic DEX fixtures for member identity/cache experiments.

Methods are abstract unless explicit code assemblers are provided. These
files exercise parsing/query behavior, not Android application execution.
The APK is a DEX ZIP container with no application manifest.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import zlib


def uleb(value):
    result = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        result.append(byte | (128 if value else 0))
        if not value:
            return result


def mutf8(text):
    units = text.encode('utf-16-le', errors='surrogatepass')
    result = bytearray(uleb(len(units) // 2))
    for (unit,) in struct.iter_unpack('<H', units):
        if 0 < unit < 128:
            result.append(unit)
        elif unit < 2048:
            result.extend([0xc0 | (unit >> 6), 0x80 | (unit & 63)])
        else:
            result.extend([0xe0 | (unit >> 12), 0x80 | ((unit >> 6) & 63), 0x80 | (unit & 63)])
    return result + b'\0'


def shorty(proto):
    return ''.join('L' if value[0] in '[L' else value for value in (proto[0], *proto[1]))


def make_dex(classes, methods, fields, references=(), field_references=(), extras=(),
             code=None, source_files=None, additional_strings=()):
    # method tuple: owner, name, return type, ordered parameter tuple
    all_methods = set(methods) | set(references)
    all_fields = set(fields) | set(field_references)
    type_names = {'Ljava/lang/Object;', *classes, *extras}
    for owner, name, ret, params in all_methods:
        type_names.update([owner, ret, *params])
    for owner, name, field_type in all_fields:
        type_names.update([owner, field_type])
    for interfaces in classes.values():
        type_names.update(interfaces)
    protos = {(ret, params) for _, _, ret, params in all_methods}
    strings = {'', 'Fixture.java', *type_names}
    strings.update(additional_strings)
    if source_files:
        strings.update(value for value in source_files.values() if value is not None)
    strings.update(name for _, name, _, _ in all_methods)
    strings.update(name for _, name, _ in all_fields)
    strings.update(shorty(proto) for proto in protos)
    strings = sorted(strings, key=lambda value: value.encode('utf-16-be'))
    string_ids = {value: i for i, value in enumerate(strings)}
    types = sorted(type_names, key=string_ids.__getitem__)
    type_ids = {value: i for i, value in enumerate(types)}
    protos = sorted(protos, key=lambda p: (type_ids[p[0]], tuple(type_ids[t] for t in p[1])))
    proto_ids = {value: i for i, value in enumerate(protos)}
    method_list = sorted(all_methods, key=lambda m: (type_ids[m[0]], string_ids[m[1]], proto_ids[m[2:]]))
    field_list = sorted(all_fields, key=lambda f: (type_ids[f[0]], string_ids[f[1]], type_ids[f[2]]))
    method_ids = {value: i for i, value in enumerate(method_list)}
    field_ids = {value: i for i, value in enumerate(field_list)}
    class_list = sorted(classes, key=type_ids.__getitem__)
    image = bytearray(112)
    sections = []

    def fixed(item_type, values, width):
        offset = len(image) if values else 0
        if values:
            sections.append((item_type, len(values), offset))
            image.extend(bytes(len(values) * width))
        return offset

    string_off = fixed(1, strings, 4)
    type_off = fixed(2, types, 4)
    proto_off = fixed(3, protos, 12)
    field_off = fixed(4, field_list, 8)
    method_off = fixed(5, method_list, 8)
    class_off = fixed(6, class_list, 32)
    data_off = len(image)
    sections.append((0x2002, len(strings), len(image)))
    for i, value in enumerate(strings):
        struct.pack_into('<I', image, string_off + 4 * i, len(image))
        image.extend(mutf8(value))
    for i, value in enumerate(types):
        struct.pack_into('<I', image, type_off + 4 * i, string_ids[value])

    def align():
        image.extend(bytes((-len(image)) % 4))

    lists = {p[1] for p in protos if p[1]}
    lists.update(tuple(sorted(values, key=type_ids.__getitem__)) for values in classes.values() if values)
    list_offsets = {(): 0}
    for values in sorted(lists, key=lambda values: tuple(type_ids[t] for t in values)):
        align()
        if not any(kind == 0x1001 for kind, _, _ in sections):
            sections.append((0x1001, len(lists), len(image)))
        list_offsets[values] = len(image)
        image.extend(struct.pack('<I', len(values)))
        image.extend(struct.pack('<' + 'H' * len(values), *(type_ids[t] for t in values)))
    for i, proto in enumerate(protos):
        struct.pack_into('<III', image, proto_off + 12 * i,
                         string_ids[shorty(proto)], type_ids[proto[0]], list_offsets[proto[1]])
    for i, (owner, name, field_type) in enumerate(field_list):
        struct.pack_into('<HHI', image, field_off + 8 * i, type_ids[owner], type_ids[field_type], string_ids[name])
    for i, (owner, name, ret, params) in enumerate(method_list):
        struct.pack_into('<HHI', image, method_off + 8 * i, type_ids[owner], proto_ids[(ret, params)], string_ids[name])

    code_offsets = {}
    for method, assembler in (code or {}).items():
        if method not in methods or method[2:] != ('V', ()):
            raise ValueError('Code fixtures currently support defined static ()V methods only.')
        units = assembler(method_ids, field_ids, string_ids)
        align()
        if not code_offsets:
            sections.append((0x2001, len(code), len(image)))
        code_offsets[method_ids[method]] = len(image)
        image.extend(struct.pack('<HHHHII', 2, 0, 0, 0, 0, len(units)))
        image.extend(struct.pack('<' + 'H' * len(units), *units))

    class_data_count = 0
    class_data_start = 0
    for i, owner in enumerate(class_list):
        owned_methods = sorted(method_ids[m] for m in methods if m[0] == owner)
        owned_fields = sorted(field_ids[f] for f in fields if f[0] == owner)
        class_data_off = 0
        if owned_methods or owned_fields:
            class_data_off = len(image)
            if not class_data_count:
                class_data_start = class_data_off
            class_data_count += 1
            direct_methods = [idx for idx in owned_methods if idx in code_offsets]
            virtual_methods = [idx for idx in owned_methods if idx not in code_offsets]
            for count in [len(owned_fields), 0, len(direct_methods), len(virtual_methods)]:
                image.extend(uleb(count))
            previous = 0
            for idx in owned_fields:
                image.extend(uleb(idx - previous) + uleb(0x9))  # public static
                previous = idx
            previous = 0
            for idx in direct_methods:
                image.extend(uleb(idx - previous) + uleb(0x9) + uleb(code_offsets[idx]))  # public static
                previous = idx
            previous = 0
            for idx in virtual_methods:
                image.extend(uleb(idx - previous) + uleb(0x401) + uleb(0))  # public abstract, no code
                previous = idx
        interfaces = tuple(sorted(classes[owner], key=type_ids.__getitem__))
        flags = 0x601 if owner.startswith('Lfixture/I') and owner != 'Lfixture/Implementor;' else 0x401
        source = (source_files or {}).get(owner, 'Fixture.java')
        struct.pack_into('<8I', image, class_off + 32 * i,
                         type_ids[owner], flags, type_ids['Ljava/lang/Object;'], list_offsets[interfaces],
                         string_ids[source] if source is not None else 0xffffffff, 0, class_data_off, 0)
    if class_data_count:
        sections.append((0x2000, class_data_count, class_data_start))
    align()
    map_off = len(image)
    sections.extend([(0, 1, 0), (0x1000, 1, map_off)])
    sections.sort(key=lambda item: item[2])
    image.extend(struct.pack('<I', len(sections)))
    for kind, count, offset in sections:
        image.extend(struct.pack('<HHII', kind, 0, count, offset))
    image[:8] = b'dex\n035\0'
    struct.pack_into('<20I', image, 32, len(image), 112, 0x12345678, 0, 0, map_off,
                     len(strings), string_off, len(types), type_off, len(protos), proto_off,
                     len(field_list), field_off, len(method_list), method_off, len(class_list), class_off,
                     len(image) - data_off, data_off)
    image[12:32] = hashlib.sha1(image[32:]).digest()
    struct.pack_into('<I', image, 8, zlib.adler32(image[12:]) & 0xffffffff)
    return bytes(image), {
        'methods': [{'id': i, 'descriptor': owner + '->' + name + '(' + ''.join(params) + ')' + ret,
                     'defined': (owner, name, ret, params) in methods}
                    for i, (owner, name, ret, params) in enumerate(method_list)],
        'fields': [{'id': i, 'descriptor': owner + '->' + name + ':' + field_type,
                    'defined': (owner, name, field_type) in fields}
                   for i, (owner, name, field_type) in enumerate(field_list)],
        'class_interfaces': classes,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--wide-methods', type=int, default=4096)
    parser.add_argument('--same-name-overloads', action='store_true')
    args = parser.parse_args()
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit('Choose an empty fixture output directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    api = 'Lfixture/Api;'
    wide = 'Lfixture/Wide;'
    long_type = 'Lfixture/' + 'LongParameterType' * 8 + ';'
    interfaces = [f'Lfixture/I{i};' for i in range(8)]
    classes = {api: [], wide: [], 'Lfixture/Other;': [], 'Lfixture/Empty;': [], 'Lfixture/Implementor;': interfaces}
    classes.update({name: [] for name in interfaces})
    methods = {
        (api, 'a', 'I', ()), (api, 'a', 'J', ()), (api, 'a', 'I', ('I',)),
        (api, 'a', 'I', ('[I',)), (api, 'a', 'I', ('[[I',)),
        (api, 'a', 'V', ('[Ljava/lang/String;', 'I')),
        (api, 'a', 'V', ('I', '[Ljava/lang/String;')),
        (api, 'c', 'Ljava/lang/String;', ()), (api, '\u03bb', 'V', ('Lfixture/\u03a9;',)),
        ('Lfixture/Other;', 'a', 'I', ()),
    }
    if args.same_name_overloads:
        methods.update((wide, 'overload', 'V', (long_type,) * 12 + (f'Lfixture/Tail{i:05d};',))
                       for i in range(args.wide_methods))
    else:
        methods.update((wide, f'm{i:05d}', 'V', (long_type,) * 12 + ('I',)) for i in range(args.wide_methods))
    fields = {(api, 'a', 'I'), (api, 'a', 'J'), (api, 'c', '[I'), (api, '\u03bb', 'Lfixture/\u03a9;'),
              ('Lfixture/Other;', 'a', 'I')}
    definition, definition_info = make_dex(classes, methods, fields)
    references = {m for m in methods if m[0] == api} | {(api, 'b_missing', 'V', ())}
    field_refs = {f for f in fields if f[0] == api} | {(api, 'b_missing', 'I'), ('Laaa/Extra;', 'extra', 'I')}
    references.add(('Laaa/Extra;', 'extra', 'V', ()))
    client, client_info = make_dex({'Lfixture/Client;': []}, {('Lfixture/Client;', 'run', 'V', ())}, set(),
                                 references, field_refs, extras=['Laaa/Extra;'])
    duplicate_methods = {m for m in methods if m[0] == api}
    duplicate, duplicate_info = make_dex({api: []}, duplicate_methods, {f for f in fields if f[0] == api}, extras=['Laaa/Another;'])
    manifest = {'scope': 'Metadata-only fixtures; abstract methods have no executable code.', 'dexes': []}
    with zipfile.ZipFile(args.output / 'symbols.apk', 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for number, (data, info) in enumerate([(definition, definition_info), (client, client_info), (duplicate, duplicate_info)], 1):
            name = 'classes' + (str(number) if number > 1 else '') + '.dex'
            entry = zipfile.ZipInfo(name, date_time=(2020, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, data)
            (args.output / name).write_bytes(data)
            info.update({'name': name, 'sha256': hashlib.sha256(data).hexdigest()})
            manifest['dexes'].append(info)
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=True) + '\n')
    print(args.output / 'symbols.apk')


if __name__ == '__main__':
    main()
