#!/usr/bin/env python3
"""Build an immutable native snapshot with its source and compiler manifest."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--java-home', type=Path, required=True)
    parser.add_argument('--define', action='append', default=[])
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    root, output = args.source_root.resolve(), args.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise SystemExit('Native snapshots are immutable; choose a new output directory.')
    output.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env['JAVA_HOME'] = str(args.java_home.resolve())
    env['PATH'] = env['JAVA_HOME'] + '/bin:' + env['PATH']
    before = git(root, 'rev-parse', 'HEAD')
    source_paths = ['Core', 'dexkit/src/main/cpp', 'schema']
    diff = git(root, 'diff', 'HEAD', '--', *source_paths)
    (output / 'source.patch').write_text(diff + '\n')
    # Record untracked native files too; experiment diagnostics may be new files.
    untracked = git(root, 'ls-files', '--others', '--exclude-standard', '--', *source_paths).splitlines()
    untracked_hashes = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in untracked}
    for path in untracked:
        saved = output / 'source-extra' / path
        saved.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(root / path, saved)
    configure = ['cmake', '-S', str(root / 'dexkit/src/main/cpp'), '-B', str(output / 'build'),
                 '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64',
                 '-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG', '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG',
                 '-DDEXKIT_ENABLE_INTERNAL_METRICS=OFF', '-DDEXKIT_ENABLE_INTERNAL_METRICS_API=OFF']
    configure += ['-D' + value for value in args.define]
    print('Building native snapshot:', output, flush=True)
    with (output / 'build.log').open('w') as log:
        for cmd in [configure, ['cmake', '--build', str(output / 'build'), '--parallel', str(args.jobs)]]:
            result = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                print('\n'.join((output / 'build.log').read_text().splitlines()[-70:]))
                raise SystemExit(result.returncode)
    if before != git(root, 'rev-parse', 'HEAD') or diff != git(root, 'diff', 'HEAD', '--', *source_paths):
        raise SystemExit('Source changed during build; do not use this artifact.')
    if any(hashlib.sha256((root / p).read_bytes()).hexdigest() != value for p, value in untracked_hashes.items()):
        raise SystemExit('Untracked source changed during build; do not use this artifact.')
    if untracked != git(root, 'ls-files', '--others', '--exclude-standard', '--', *source_paths).splitlines():
        raise SystemExit('Untracked source list changed during build; do not use this artifact.')
    library = output / 'libdexkit.dylib'
    shutil.copyfile(output / 'build/libdexkit.dylib', library)
    cache = (output / 'build/CMakeCache.txt').read_text()
    (output / 'CMakeCache.txt').write_text(cache)
    settings = {}
    for line in cache.splitlines():
        if line.startswith(('DEXKIT_', 'CMAKE_CXX_COMPILER:', 'CMAKE_C_COMPILER:',
                            'CMAKE_CXX_FLAGS', 'CMAKE_C_FLAGS', 'CMAKE_OSX_', 'CMAKE_BUILD_TYPE:')) and '=' in line:
            key, value = line.split('=', 1)
            settings[key.split(':')[0]] = value
    compiler = settings['CMAKE_CXX_COMPILER']
    manifest = {'engine_commit': before,
                'source_trees': {p: git(root, 'rev-parse', f'HEAD:{p}') for p in source_paths},
                'source_patch_sha256': hashlib.sha256((output / 'source.patch').read_bytes()).hexdigest(),
                'untracked_source_sha256': untracked_hashes,
                'native_sha256': hashlib.sha256(library.read_bytes()).hexdigest(),
                'cmake_options': settings, 'configure_command': configure,
                'compiler': subprocess.check_output([compiler, '--version'], text=True).strip(),
                'diagnostics': any(settings.get(key, 'OFF') == 'ON' for key in
                                   ['DEXKIT_BENCHMARK_DIAGNOSTICS', 'DEXKIT_BENCHMARK_STRING_TRACE',
                                    'DEXKIT_BENCHMARK_BATCH_TRACE'])}
    (output / 'artifact.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Native SHA256:', manifest['native_sha256'])


if __name__ == '__main__':
    main()
