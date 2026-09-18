import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path('/Users/teble/project/android/DexKit-qaux-benchmark')
E = Path('/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/narrow-types-v1/r3')
JDK = '/Users/teble/Library/Java/JavaVirtualMachines/jbr-17.0.6/Contents/Home'
SMALL = ['LAZY_DIRECTORIES', 'COMPACT_STRINGS', 'NEGATIVE_STRINGS', 'STRUCTURAL_DESCRIPTORS',
         'DESCRIPTOR_FAST_HITS', 'RAW_INTERFACES', 'FIELD_IDENTITY_SPLIT', 'COMPACT_INVOKES',
         'SINGLE_RELATION', 'INVERTED_STRINGS', 'INVERTED_STRING_RANGES',
         'SKIP_EMPTY_CANDIDATES', 'MOVE_FIND_RESULTS', 'COMPACT_CALLERS']
STORAGE = ['NODE_DESCRIPTORS', 'SPARSE_DESCRIPTORS', 'HYBRID_DESCRIPTORS', 'VECTOR_DESCRIPTORS', 'UNCACHED_DESCRIPTORS']
FLAGS = {'control': False, 'narrow': True}
phase = sys.argv[1]
plans = {
    'normal': [('control', 'control', False, True, False), ('narrow', 'narrow', False, True, False)],
    'trace': [('control-trace', 'control', True, True, False), ('narrow-trace', 'narrow', True, True, False)],
    'isolated': [('isolated-narrow', 'narrow', True, False, False), ('default-off', 'control', True, False, False)],
    'sanitize': [('narrow-sanitized', 'narrow', True, True, True)],
    'combination': [('narrow-fields', 'narrow', True, True, False)],
}


def run(spec):
    name, selected, trace, small, sanitize = spec
    command = ['python3', 'tools/qaux-benchmark/build_native.py', '--source-root', str(ROOT),
               '--output', str(E / 'artifacts' / name), '--java-home', JDK, '--jobs', '3']
    definitions = ['DEXKIT_BENCHMARK_DESCRIPTOR_WORKLOAD=ON', 'DEXKIT_BENCHMARK_RELATION_WORKLOAD=ON',
                   'DEXKIT_BENCHMARK_FIELD_WORKLOAD=ON', 'DEXKIT_BENCHMARK_STRING_WORKLOAD=ON',
                   'DEXKIT_BENCHMARK_BATCH_WORKLOAD=ON']
    if trace:
        definitions += ['DEXKIT_BENCHMARK_SYMBOL_CHECKS=ON', 'DEXKIT_BENCHMARK_RELATION_CHECKS=ON']
    if small:
        definitions += ['DEXKIT_EXPERIMENT_' + flag + '=ON' for flag in SMALL]
    definitions += ['DEXKIT_EXPERIMENT_' + flag + '=' + ('ON' if small and flag == 'UNCACHED_DESCRIPTORS' else 'OFF') for flag in STORAGE]
    if small:
        definitions += ['DEXKIT_EXPERIMENT_RAW_DESCRIPTOR_LOOKUP=ON']
    definitions += ['DEXKIT_BENCHMARK_DIAGNOSTICS=' + ('ON' if trace else 'OFF'),
                    'DEXKIT_EXPERIMENT_NARROW_TYPES=' + ('ON' if FLAGS[selected] else 'OFF')]
    if trace:
        definitions += ['DEXKIT_BENCHMARK_METADATA_CHECKS=ON', 'DEXKIT_BENCHMARK_SOURCE_CHECKS=ON']
    if name == 'narrow-fields':
        definitions += ['DEXKIT_EXPERIMENT_COMPACT_FIELDS=ON', 'DEXKIT_EXPERIMENT_PACKED_FIELD_USES=ON',
                        'DEXKIT_EXPERIMENT_PACKED_CROSS_INFO=ON']
    if sanitize:
        flags = '-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
        definitions += ['CMAKE_C_FLAGS_RELEASE=' + flags, 'CMAKE_CXX_FLAGS_RELEASE=' + flags,
                        'CMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined',
                        'CMAKE_SHARED_LINKER_FLAGS=-fsanitize=address,undefined']
    for definition in definitions:
        command += ['--define', definition]
    sources = {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
               for path in (ROOT/'tools/qaux-benchmark').glob('*.cpp')}
    print('START ' + name, flush=True)
    with (E/'validation'/(name+'-build-launcher.log')).open('w') as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    assert all(hashlib.sha256((ROOT/path).read_bytes()).hexdigest() == sha for path, sha in sources.items())
    (E/'artifacts'/name/'workload-sources.json').write_text(json.dumps(sources, indent=2)+'\n')
    print('DONE ' + name, flush=True)
    return dict(name=name, command=command)


with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    result = list(pool.map(run, plans[phase]))
(E/'validation'/('build-'+phase+'.json')).write_text(json.dumps(result, indent=2)+'\n')
