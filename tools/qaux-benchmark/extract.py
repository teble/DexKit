#!/usr/bin/env python3
"""Extract literal QAuxiliary string targets without executing Android code."""

import argparse
import base64
import collections
import json
from pathlib import Path
import re
import subprocess


def without_comments(text):
    pattern = r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda m: m[0] if m[0].startswith('"') else
                  re.sub(r'[^\n]', ' ', m[0]), text)


def literal_array(text, start):
    """Parse only arrayOf/string literals; fail on interpolation or expressions."""
    token = re.compile(r'\s*(arrayOf|\(|\)|,|"(?:\\.|[^"\\])*")')

    def take(pos):
        match = token.match(text, pos)
        if not match:
            raise ValueError('nonliteral expression at ' + repr(text[pos:pos + 60]))
        return match[1], match.end()

    def parse(pos):
        value, pos = take(pos)
        if value.startswith('"'):
            if re.search(r'(?<!\\)\$(?:[A-Za-z_]|\{)', value):
                raise ValueError('Kotlin interpolation requires a host-specific adapter')
            # Kotlin supports escaped dollar signs, unlike JSON.
            return json.loads(value.replace('\\$', '$')), pos
        if value != 'arrayOf':
            raise ValueError('expected arrayOf')
        value, pos = take(pos)
        if value != '(':
            raise ValueError('expected opening parenthesis')
        values = []
        while True:
            value, next_pos = take(pos)
            if value == ')':
                return values, next_pos
            value, pos = parse(pos)
            values.append(value)
            sep, pos = take(pos)
            if sep == ')':
                return values, pos
            if sep != ',':
                raise ValueError('expected comma')

    return parse(start)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qaux-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.qaux_root.resolve()
    source_path = 'app/src/main/java/io/github/qauxv/util/dexkit/DexKitTarget.kt'
    raw = (root / source_path).read_text()
    source = without_comments(raw)
    commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
    objects = list(re.finditer(r'data object (\w+)\s*:\s*DexKitTarget\.(\w+)\(\)', source))
    targets, skipped, kinds = [], [], collections.Counter()
    for index, obj in enumerate(objects):
        name, kind = obj.groups()
        kinds[kind] += 1
        if kind not in ('UsingStr', 'UsingStringVector'):
            continue
        end = objects[index + 1].start() if index + 1 < len(objects) else len(source)
        block = source[obj.end():end]
        prop = re.search(r'override\s+val\s+(traitStringVectors|traitString)\b[^=]*=', block)
        line = source.count('\n', 0, obj.start()) + 1
        try:
            if prop is None:
                raise ValueError('missing literal trait property')
            values, value_end = literal_array(block, prop.end())
            tail = block[value_end:].lstrip()
            if not (tail.startswith('override') or tail.startswith('}')):
                raise ValueError('expression following literal array requires review')
            if kind == 'UsingStr':
                if not all(isinstance(v, str) for v in values):
                    raise ValueError('UsingStr requires a flat string array')
                groups = [[value] for value in values]
            else:
                groups = values
                if not all(isinstance(v, list) and v and all(isinstance(s, str) for s in v) for v in groups):
                    raise ValueError('UsingStringVector requires nested string arrays')
            if not groups:
                raise ValueError('empty target')
            groups = [list(dict.fromkeys(group)) for group in groups]
            targets.append({'name': name, 'kind': kind, 'source_line': line,
                            'source_url': f'https://github.com/cinit/QAuxiliary/blob/{commit}/{source_path}#L{line}',
                            'groups': groups, 'host_filter_applied': False})
        except ValueError as exc:
            skipped.append({'name': name, 'source_line': line, 'reason': str(exc)})
    args.output.mkdir(parents=True, exist_ok=True)
    record = {'qaux_commit': commit, 'source_file': source_path,
              'scope': 'All literal target definitions; raw candidates only. This is not a captured enabled-feature profile.',
              'string_match_type': 'SimilarRegex', 'object_kinds': dict(kinds),
              'targets': targets, 'skipped': skipped}
    (args.output / 'targets.json').write_text(json.dumps(record, indent=2, ensure_ascii=True) + '\n')
    with (args.output / 'groups.tsv').open('w') as stream:
        for target in targets:
            for index, group in enumerate(target['groups']):
                encoded = [base64.b64encode(s.encode()).decode() for s in group]
                stream.write('\t'.join([f"{target['name']}#_#{index}", *encoded]) + '\n')
    # Keep the source attribution with generated local data.
    (args.output / 'SOURCE-NOTICE.txt').write_text(raw.split('package ', 1)[0])
    print(json.dumps({'qaux_commit': commit, 'object_kinds': dict(kinds),
                      'extracted_targets': len(targets),
                      'groups': sum(len(t['groups']) for t in targets),
                      'skipped': skipped}, indent=2))
    if skipped:
        raise SystemExit('Some definitions require manual review; see targets.json.')


if __name__ == '__main__':
    main()
