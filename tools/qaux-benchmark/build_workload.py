#!/usr/bin/env python3
"""Link the native workload against an immutable, already built core archive."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--native-artifact', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root, native, output = (p.resolve() for p in (args.source_root, args.native_artifact, args.output))
    if output.exists() and any(output.iterdir()):
        raise SystemExit('Use a new immutable output directory.')
    manifest = json.loads((native / 'artifact.json').read_text())
    if manifest['diagnostics'] or manifest['native_sha256'] != sha(native / 'libdexkit.dylib'):
        raise SystemExit('Require the original non-diagnostic native artifact.')
    paths = list(manifest['source_trees'])
    if git(root, 'diff', 'HEAD', '--', *paths) or git(root, 'ls-files', '--others', '--exclude-standard', '--', *paths):
        raise SystemExit('Core/header source must be committed and unchanged.')
    if any(git(root, 'rev-parse', 'HEAD:' + path) != tree for path, tree in manifest['source_trees'].items()):
        raise SystemExit('Headers must match the compiled native source trees.')
    source = root / 'tools/qaux-benchmark/descriptor_workload.cpp'
    archive = native / 'build/Core/libdexkit_static.a'
    source_hash, archive_hash, commit = sha(source), sha(archive), git(root, 'rev-parse', 'HEAD')
    executable = output / 'build/Core/dexkit_descriptor_workload'
    executable.parent.mkdir(parents=True, exist_ok=True)
    options = manifest['cmake_options']
    command = [options['CMAKE_CXX_COMPILER'], '-std=c++20', '-O3', '-DNDEBUG', '-pthread', '-arch', 'arm64']
    definitions = ['DEXKIT_BENCHMARK_DIAGNOSTICS', 'DEXKIT_ENABLE_INTERNAL_METRICS']
    definitions += [name for name in options if name.startswith('DEXKIT_EXPERIMENT_')]
    for name in definitions:
        value = options[name]
        value = {'ON': '1', 'OFF': '0', 'TRUE': '1', 'FALSE': '0'}.get(value.upper(), value)
        command.append('-D' + name + '=' + value)
    for directory in ['dexkit/include', 'third_party/slicer/export', 'third_party/thread_helper',
                      'third_party/aho_corasick_trie', 'third_party/parallel_hashmap', 'third_party/flatbuffers/include']:
        command += ['-I', str(root / 'Core' / directory)]
    command += [str(source), str(archive), '-lz', '-o', str(executable)]
    with (output / 'build.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    if sha(source) != source_hash or sha(archive) != archive_hash or git(root, 'rev-parse', 'HEAD') != commit:
        raise SystemExit('Source or archive changed while linking.')
    shutil.copyfile(native / 'libdexkit.dylib', output / 'libdexkit.dylib')
    manifest['workload'] = dict(source_commit=commit, source_sha256=source_hash,
        core_archive_sha256=archive_hash, executable_sha256=sha(executable), command=command,
        native_artifact=str(native))
    (output / 'artifact.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(executable)


if __name__ == '__main__':
    main()
