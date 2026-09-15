#!/usr/bin/env python3
"""Inspect string witnesses without changing the frozen replay or native code.

This is a compatibility/corpus diagnostic, not a performance measurement.
Generated adapter copies retain QueryReplay.java's source/license notice.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


WITNESS_HELPER = '''
    private static List<Object> auditWitnesses(Collection<?> values) {
        List<Object> result = new ArrayList<>();
        for (Object value : values) {
            List<String> strings = new ArrayList<>();
            String identity;
            if (value instanceof MethodData) {
                MethodData method = (MethodData) value;
                identity = method.getDescriptor();
                strings.addAll(method.getUsingStrings());
            } else {
                ClassData cls = (ClassData) value;
                identity = cls.getName();
                for (MethodData method : cls.getMethods()) {
                    strings.addAll(method.getUsingStrings());
                }
            }
            result.add(obj("identity", identity, "strings", strings));
        }
        return result;
    }

'''


def transformed(source):
    anchor = '    private static final List<Object> stages = new ArrayList<>();'
    assert source.count(anchor) == 1
    source = source.replace(anchor, WITNESS_HELPER + anchor)
    # Only default ordinary matcher calls, not batch groups or addEqString.
    pattern = re.compile(r'\.usingStrings\(((?:"(?:\\.|[^"\\])*"\s*,?\s*)+)\)')
    source, count = pattern.subn(
        r'.usingStrings(List.of(\1), StringMatchType.valueOf('
        r'System.getProperty("qaux.audit.mode", "Contains")), false)', source)
    if count != 12:
        raise ValueError(f'Review adapter changes: expected 12 literal calls, saw {count}')
    anchor = '            record.put("ordered_results", values);'
    assert source.count(anchor) == 1
    source = source.replace(anchor, anchor + '\n            record.put("audit_witnesses", auditWitnesses(result));')
    anchor = '"multiset_sha256", digest(values)));'
    assert source.count(anchor) == 1
    source = source.replace(anchor,
        '"multiset_sha256", digest(values), "audit_witnesses", '
        'auditWitnesses(methods == null ? Collections.emptyList() : methods)));')
    return source, count


def signature(report):
    stages = []
    for stage in report['stages']:
        row = {'feature': stage['feature'], 'stage': stage['stage']}
        if 'groups' in stage:
            row['groups'] = {k: v['ordered_results'] for k, v in stage['groups'].items()}
            row['returned_keys'] = stage['returned_keys']
        else:
            row['ordered_results'] = stage['ordered_results']
        stages.append(row)
    features = [{k: v for k, v in row.items() if k != 'observed_ns'}
                for row in report['features']]
    return {'stages': stages, 'features': features, 'api_errors': report['api_errors']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference-run', type=Path, required=True,
                        help='Existing measurement metadata with frozen library/JDK/APK identities')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise SystemExit('Choose an empty output directory.')
    metadata = json.loads((args.reference_run / 'run-metadata.json').read_text())
    root = Path(__file__).resolve().parent
    source_path = root / 'QueryReplay.java'
    generated, call_count = transformed(source_path.read_text())
    java = next(Path(p) for p in metadata['java_identity'] if p.endswith('/bin/java'))
    jars = [Path(p) for p in metadata['jars_sha256']]
    native_arg = next(v for v in metadata['command'] if v.startswith('-Djava.library.path='))
    native = Path(native_arg.split('=', 1)[1]) / 'libdexkit.dylib'
    apk = Path(metadata['command'][metadata['command'].index('QueryReplay') + 1])
    groups = Path(metadata['command'][metadata['command'].index('QueryReplay') + 2])
    expected_hashes = {**metadata['java_identity'], **metadata['jars_sha256'],
                       str(native): metadata['native_sha256'], str(apk): metadata['apk_sha256'],
                       str(groups): metadata['groups_sha256']}
    for name, expected in expected_hashes.items():
        if sha256(Path(name)) != expected:
            raise SystemExit('Pinned input changed: ' + name)
    adapter = output / 'QueryReplay.java'
    adapter.write_text(generated)
    classes = output / 'classes'
    classes.mkdir()
    classpath = ':'.join(map(str, jars))
    subprocess.run([str(java.with_name('javac')), '--release', '11', '-cp', classpath,
                    '-d', str(classes), str(adapter)], check=True)
    reports, commands = {}, {}
    for mode in ('Contains', 'Equals', 'StartsWith'):
        destination = output / f'{mode}.json'
        cmd = [str(java), '-Xms256m', '-Xmx512m', native_arg,
               f'-Dqaux.audit.mode={mode}', '-cp', str(classes) + ':' + classpath,
               'QueryReplay', str(apk), str(groups), str(destination), '4', '1', 'all', 'verify']
        commands[mode] = cmd
        with (output / f'{mode}.stdout').open('w') as stdout, (output / f'{mode}.stderr').open('w') as stderr:
            subprocess.run(cmd, stdout=stdout, stderr=stderr, timeout=300, check=True)
        reports[mode] = json.loads(destination.read_text())
        print(json.dumps({'mode': mode, 'stages': len(reports[mode]['stages']),
                          'api_errors': reports[mode]['api_errors']}), flush=True)
    frozen = json.loads((Path(metadata['verified_run']) / 'report.json').read_text())
    frozen['stages'] = [s for s in frozen['stages'] if s['pass'] == 0]
    frozen['features'] = [s for s in frozen['features'] if s['pass'] == 0]
    assert signature(reports['Contains']) == signature(frozen), 'Diagnostic adapter changed baseline results'
    base = signature(reports['Contains'])
    equality = {mode: signature(report) == base for mode, report in reports.items()}
    manifest = {'scope': 'Pinned APK witness diagnostic; timing fields are not performance evidence.',
                'source_adapter_sha256': sha256(source_path),
                'generated_adapter_sha256': sha256(adapter), 'audit_script_sha256': sha256(Path(__file__)),
                'changed_literal_call_sites': call_count, 'pinned_inputs': expected_hashes,
                'ordinary_matcher_modes_only': True, 'batch_groups_unchanged': True,
                'diagnostic_contains_matches_frozen_order_and_flow': True,
                'ordinary_narrowing_same_order_and_flow': equality,
                'commands': commands,
                'outputs': {mode: sha256(output / f'{mode}.json') for mode in reports}}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(json.dumps(equality), flush=True)


if __name__ == '__main__':
    main()
