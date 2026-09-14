#!/usr/bin/env python3
"""Compile and replay the pinned QQ 9.3.55 QAuxiliary query corpus."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import signal
import shutil
import subprocess
import sys
import time

QAUX_COMMIT = '01801ffd013c95781dd360704adf48dc42ee8aa6'
APK_SHA256 = '851242d139bb01ed8c787eadc30d7ec391437de550c697b5f4c65c32ec84286f'


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def cached_jar(cache, group, artifact, version):
    candidates = list((cache / group / artifact / version).glob(f'*/{artifact}-{version}.jar'))
    if len(candidates) != 1:
        raise ValueError(f'Expected one cached {artifact}:{version}, got {candidates}. Build :dexkit:jar first.')
    return candidates[0]


def fingerprints(report, counts_only=False):
    by_pass = {}
    for row in report['stages']:
        key = (row['feature'], row['stage'])
        values = by_pass.setdefault(row['pass'], {})
        values.setdefault(('workflow', 'stage_sequence'), []).append(key)
        if 'groups' in row:
            values[key] = {name: value['count'] if counts_only else (value['count'], value['multiset_sha256'])
                           for name, value in row['groups'].items()}
            if 'returned_keys' in row:
                values[(row['feature'], row['stage'] + '_returned_keys')] = row['returned_keys']
        else:
            values[key] = row['count'] if counts_only else (row['count'], row['multiset_sha256'])
    for row in report['features']:
        values = by_pass.setdefault(row['pass'], {})
        values.setdefault(('workflow', 'feature_sequence'), []).append(row['feature'])
        values[(row['feature'], 'outcome')] = (
            row['status'], row.get('selected'), row.get('reason'))
    return by_pass


def expected_fingerprint(expected, profile, counts_only=False):
    stages = [dict(row, **{'pass': 0}) for row in expected['stages']
              if profile == 'all' or (row['feature'] == 'all_literal_targets') == (profile == 'batch')]
    features = [dict(row, **{'pass': 0}) for row in expected['features']] if profile != 'batch' else []
    return fingerprints({'stages': stages, 'features': features}, counts_only)[0]


def validate_report(report, profile, mode, passes, threads):
    required = {'profile': profile, 'mode': mode, 'passes': passes,
                'threads': threads, 'dex_num': 41, 'api_errors': 0, 'completed': True}
    for key, value in required.items():
        if report.get(key) != value:
            raise ValueError(f'Report {key} differs: expected {value!r}, got {report.get(key)!r}')
    for key in ['create_ns', 'close_ns', 'create_to_close_observed_ns']:
        if not isinstance(report.get(key), int) or report[key] < 0:
            raise ValueError(f'Missing or invalid lifecycle timing: {key}')
    wanted_passes = set(range(passes))
    if {row['pass'] for row in report['stages']} != wanted_passes:
        raise ValueError('Report has missing or unexpected query passes')
    if profile not in ('batch', 'diagnostics'):
        if {row['pass'] for row in report['features']} != wanted_passes:
            raise ValueError('Report has missing or unexpected feature passes')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dexkit-root', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--apk', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--java-home', type=Path)
    parser.add_argument('--threads', type=int, default=4)
    parser.add_argument('--passes', type=int, default=2)
    parser.add_argument('--profile', choices=['all', 'chains', 'batch', 'diagnostics'], default='all')
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--expected', type=Path, default=Path(__file__).with_name('baseline') / 'expected.json')
    parser.add_argument('--mode', choices=['verify', 'measure'], default='verify')
    parser.add_argument('--verified-run', type=Path)
    parser.add_argument('--native-library', type=Path)
    parser.add_argument('--memory-probe', type=Path)
    args = parser.parse_args()
    root, corpus, apk, output = [p.resolve() for p in
                                  (args.dexkit_root, args.corpus, args.apk, args.output)]
    if output.exists() and any(output.iterdir()):
        raise SystemExit('Choose a new output directory; reports are not overwritten.')
    output.mkdir(parents=True, exist_ok=True)
    targets = json.loads((corpus / 'targets.json').read_text())
    if targets['qaux_commit'] != QAUX_COMMIT:
        raise SystemExit('QAux source changed; review the manual adapters before replay.')
    expected = json.loads(args.expected.read_text())
    if expected['apk_sha256'] != APK_SHA256 or expected['qaux_commit'] != QAUX_COMMIT:
        raise SystemExit('Expected results refer to different pinned inputs.')
    if sha256(corpus / 'groups.tsv') != expected['groups_sha256']:
        raise SystemExit('String groups differ from the frozen baseline.')
    apk_hash = sha256(apk)
    if apk_hash != APK_SHA256:
        raise SystemExit('APK differs from the pinned QQ 9.3.55 sample; review version-dependent paths first.')
    java = str(args.java_home / 'bin/java') if args.java_home else 'java'
    javac = str(args.java_home / 'bin/javac') if args.java_home else 'javac'
    injected_options = ['JAVA_TOOL_OPTIONS', 'JDK_JAVA_OPTIONS', '_JAVA_OPTIONS']
    if any(os.environ.get(key) for key in injected_options):
        raise SystemExit('Unset JAVA_TOOL_OPTIONS, JDK_JAVA_OPTIONS and _JAVA_OPTIONS for a controlled run.')
    java_executable = Path(shutil.which(java) or java).resolve()
    java_root = java_executable.parent.parent
    jvm_name = {'Darwin': 'libjvm.dylib', 'Linux': 'libjvm.so', 'Windows': 'jvm.dll'}[platform.system()]
    java_files = [java_executable, java_root / 'release', java_root / 'lib/server' / jvm_name]
    java_identity = {str(path): sha256(path) for path in java_files if path.is_file()}
    cache = Path(os.environ.get('GRADLE_USER_HOME', str(Path.home() / '.gradle'))) / 'caches/modules-2/files-2.1'
    jars = [root / 'dexkit/build/libs/dexkit.jar',
            cached_jar(cache, 'org.jetbrains.kotlin', 'kotlin-stdlib', '1.9.20'),
            cached_jar(cache, 'com.google.flatbuffers', 'flatbuffers-java', '23.5.26')]
    library_dir = root / 'dexkit/build/library'
    library_name = {'Darwin': 'libdexkit.dylib', 'Linux': 'libdexkit.so', 'Windows': 'dexkit.dll'}[platform.system()]
    library = args.native_library.resolve() if args.native_library else library_dir / library_name
    library_dir = library.parent
    if library.name != library_name:
        raise SystemExit('The snapshot library must retain its platform loader filename.')
    for path in [*jars, library]:
        if not path.is_file():
            raise SystemExit(f'Missing {path}; build :dexkit:jar first.')
    classes = output / 'classes'
    classes.mkdir(exist_ok=True)
    classpath = os.pathsep.join(map(str, jars))
    source = Path(__file__).with_name('QueryReplay.java')
    subprocess.run([javac, '--release', '11', '-cp', classpath, '-d', str(classes), str(source)], check=True)
    jvm_options = ['-Xms256m', '-Xmx256m', '-XX:+AlwaysPreTouch']
    cmd = [java, *jvm_options, f'-Xlog:gc:file={output / "gc.log"}:uptime,level,tags',
           f'-Djava.library.path={library_dir}', '-cp',
           str(classes) + os.pathsep + classpath, 'QueryReplay', str(apk),
           str(corpus / 'groups.tsv'), str(output / 'report.json'),
           str(args.threads), str(args.passes), args.profile, args.mode]
    if args.memory_probe:
        cmd.insert(1, f'-Dqaux.memory.probe={args.memory_probe.resolve()}')
    if platform.system() == 'Darwin':
        cmd = ['/usr/bin/time', '-l', *cmd]
    metadata = {'dexkit_commit': subprocess.check_output(
        ['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
        'dexkit_working_tree': subprocess.check_output(
            ['git', '-C', str(root), 'status', '--short'], text=True).strip(),
        'apk_sha256': apk_hash, 'qaux_commit': QAUX_COMMIT,
        'groups_sha256': sha256(corpus / 'groups.tsv'),
        'adapter_sha256': sha256(source), 'native_sha256': sha256(library),
        'expected_sha256': sha256(args.expected),
        'jvm_options': jvm_options,
        'java_identity': java_identity,
        'injected_java_options': {key: None for key in injected_options},
        'memory_probe_sha256': sha256(args.memory_probe) if args.memory_probe else None,
        'jars_sha256': {str(p): sha256(p) for p in jars}, 'command': cmd,
        'mode': args.mode, 'profile': args.profile, 'threads': args.threads, 'passes': args.passes,
        'purpose': 'Controlled host query-API workload; verification is separate from measurement.'}
    manifest_path = library.parent / 'artifact.json'
    if manifest_path.is_file():
        manifest = json.loads(manifest_path.read_text())
        if manifest['native_sha256'] != metadata['native_sha256']:
            raise SystemExit('Native snapshot differs from its build manifest.')
        metadata['native_artifact'] = manifest
        metadata['native_artifact_sha256'] = sha256(manifest_path)
    if args.mode == 'measure':
        if 'native_artifact' not in metadata:
            raise SystemExit('Measurement requires an immutable native build manifest from build_native.py.')
        options = manifest['cmake_options']
        if any(options.get(key, 'OFF') not in ('OFF', '0', 'FALSE') for key in
               ['DEXKIT_BENCHMARK_DIAGNOSTICS', 'DEXKIT_ENABLE_INTERNAL_METRICS',
                'DEXKIT_ENABLE_INTERNAL_METRICS_API']):
            raise SystemExit('Formal measurement requires diagnostics and internal metrics compiled out.')
        if not args.verified_run or args.profile == 'diagnostics':
            raise SystemExit('Measurement requires a successful verification run for this profile.')
        verified = json.loads((args.verified_run / 'run-metadata.json').read_text())
        if verified.get('mode') != 'verify' or verified.get('exit_code') != 0 or verified.get('baseline_results_equal') is not True:
            raise SystemExit('The supplied run did not pass full baseline verification.')
        if verified.get('passes', 0) < args.passes:
            raise SystemExit('Verification must cover at least as many passes as measurement.')
        keys = ['apk_sha256', 'qaux_commit', 'groups_sha256', 'adapter_sha256', 'native_sha256',
                'jars_sha256', 'expected_sha256', 'profile', 'threads', 'jvm_options', 'memory_probe_sha256']
        keys += ['java_identity', 'injected_java_options']
        if any(verified.get(key) != metadata[key] for key in keys):
            raise SystemExit('The verified input, binary, adapter or profile differs from this measurement.')
        metadata['verified_run'] = str(args.verified_run.resolve())
    print('Replaying queries; log:', output / 'run.log', flush=True)
    begin = time.monotonic()
    with (output / 'run.log').open('w') as log:
        process = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT,
                                   start_new_session=(os.name == 'posix'))
        try:
            metadata['exit_code'] = process.wait(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            metadata['exit_code'] = 'timeout'
            if os.name == 'posix':
                os.killpg(process.pid, signal.SIGTERM)
            else:
                process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                if os.name == 'posix':
                    os.killpg(process.pid, signal.SIGKILL)
                else:
                    process.kill()
                process.wait()
    metadata['process_wall_seconds'] = time.monotonic() - begin
    log_text = (output / 'run.log').read_text()
    if platform.system() == 'Darwin':
        match = re.search(r'^\s*(\d+)\s+maximum resident set size\s*$', log_text, re.M)
        if match:
            metadata['process_max_rss_bytes'] = int(match[1])
        match = re.search(r'^\s*(\d+)\s+peak memory footprint\s*$', log_text, re.M)
        if match:
            metadata['process_peak_footprint_bytes'] = int(match[1])
    report_file = output / 'report.json'
    report = None
    if report_file.exists():
        try:
            report = json.loads(report_file.read_text())
            validate_report(report, args.profile, args.mode, args.passes, args.threads)
        except (ValueError, KeyError, TypeError) as error:
            metadata['report_error'] = str(error)
            report = None
    else:
        metadata['report_error'] = 'The child did not produce a report.'
    if report is not None:
        values = fingerprints(report, args.mode == 'measure')
        metadata['repeat_results_equal'] = all(v == values[0] for v in values.values()) if len(values) > 1 else None
        if args.profile != 'diagnostics':
            wanted = expected_fingerprint(expected, args.profile, args.mode == 'measure')
            equal_key = 'baseline_counts_and_flow_equal' if args.mode == 'measure' else 'baseline_results_equal'
            metadata[equal_key] = all(v == wanted for v in values.values()) and bool(values)
            metadata['baseline_mismatched_keys'] = sorted({str(key) for value in values.values()
                for key in set(value) | set(wanted) if value.get(key) != wanted.get(key)})
        print('DEX count:', report['dex_num'])
        print('Create ms:', round(report['create_ns'] / 1e6, 3))
        print('Close ms:', round(report['close_ns'] / 1e6, 3))
        print('Repeat results equal:', metadata['repeat_results_equal'])
        print('Frozen baseline results equal:', metadata.get('baseline_results_equal'))
        if args.mode == 'measure': print('Measured counts and flow equal:', metadata.get('baseline_counts_and_flow_equal'))
        for row in report['features']:
            if row['pass'] == 0:
                print(row['feature'] + ': ' + row['status'] + ('; ' + row['reason'] if 'reason' in row else ''))
    (output / 'run-metadata.json').write_text(json.dumps(metadata, indent=2) + '\n')
    if metadata['exit_code'] != 0 or metadata.get('report_error'):
        if metadata.get('report_error'): print(metadata['report_error'], file=sys.stderr)
        print(log_text[-8000:], file=sys.stderr)
        raise SystemExit(1)
    if metadata.get('repeat_results_equal') is False:
        raise SystemExit('Result fingerprints or selected descriptors changed between passes.')
    if metadata.get('baseline_results_equal') is False:
        raise SystemExit('Results or query control flow differ from the frozen baseline.')
    if metadata.get('baseline_counts_and_flow_equal') is False:
        raise SystemExit('Measured counts or query control flow differ from the frozen baseline.')


if __name__ == '__main__':
    main()
