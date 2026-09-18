from pathlib import Path
import hashlib
import json
import os
import resource
import subprocess
import sys

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
ROOT = Path('/Users/teble/project/android/DexKit-qaux-benchmark')
E = Path(__file__).resolve().parent
SOURCE = E.parent/'width_api_checks.cpp'
phase = sys.argv[1]
groups = {'normal': ['control', 'narrow'], 'isolated': ['default-off', 'isolated-narrow'],
          'sanitize': ['narrow-sanitized'], 'combination': ['narrow-fields']}
env = os.environ.copy()
env['ASAN_OPTIONS'] = 'detect_leaks=0'
env['UBSAN_OPTIONS'] = 'halt_on_error=1'
rows = []
for label in groups[phase]:
    art = E/'artifacts'/label
    manifest = json.loads((art/'artifact.json').read_text())
    opts = manifest['cmake_options']
    output = E/'validation'/('width-api-'+label)
    command = [opts['CMAKE_CXX_COMPILER'], '-std=c++20', '-O2', '-DNDEBUG', '-pthread']
    if phase == 'sanitize':
        command += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    for path in ['Core/dexkit/include', 'Core/third_party/slicer/export', 'Core/third_party/thread_helper',
                 'Core/third_party/aho_corasick_trie', 'Core/third_party/parallel_hashmap',
                 'Core/third_party/flatbuffers/include']:
        command += ['-I', str(ROOT/path)]
    for key, value in opts.items():
        if key.startswith('DEXKIT_') and value in ['ON', 'OFF']:
            command += ['-D'+key+'='+('1' if value == 'ON' else '0')]
    command += ['-DDEXKIT_EXPERIMENT_STRING_MEMO_BYTES='+opts['DEXKIT_EXPERIMENT_STRING_MEMO_BYTES'],
                str(SOURCE), str(art/'build/Core/libdexkit_static.a'), '-lz', '-o', str(output)]
    with (E/'validation'/('width-api-'+label+'-compile.log')).open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    for shape in ['limit', 'wide']:
        run = [str(output), str(E.parent/'fixtures'/shape/'widths.apk'), shape]
        completed = subprocess.run(run, capture_output=True, timeout=180, env=env)
        expected = 0
        (E/'validation'/('width-api-'+label+'-'+shape+'.log')).write_bytes(completed.stdout+completed.stderr)
        assert completed.returncode == expected, (label, shape, completed.returncode, completed.stderr[-2000:])
        print(label, shape, completed.returncode, completed.stdout.decode().strip(), flush=True)
        rows.append(dict(label=label, shape=shape, exit_code=completed.returncode, expected_exit=expected,
                         command=run, compile_command=command,
                         source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                         engine_commit=manifest['engine_commit']))
(E/'validation'/('width-api-'+phase+'.json')).write_text(json.dumps(rows, indent=2)+'\n')
