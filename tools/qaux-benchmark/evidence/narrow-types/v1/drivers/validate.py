import gzip
import hashlib
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys

ROOT = Path('/Users/teble/project/android/DexKit-qaux-benchmark')
BASE = Path('/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55')
E = BASE/'narrow-types-v1/r3'
V = E/'validation'
phase = sys.argv[1]
groups = {'normal': ['control', 'narrow'],
          'trace': ['control-trace', 'narrow-trace'],
          'isolated': ['isolated-narrow', 'default-off'],
          'sanitize': ['narrow-sanitized'], 'combination': ['narrow-fields']}
labels = groups[phase]
queue = V/('checks-'+phase+'.json')
records = json.loads(queue.read_text()) if queue.exists() else []
env = os.environ.copy()
if phase == 'sanitize':
    env['ASAN_OPTIONS'] = 'detect_leaks=0'
    env['UBSAN_OPTIONS'] = 'halt_on_error=1'
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(name, command, binary=False, expected_exit=0):
    path = V/(name+('.bin' if binary else '.log'))
    previous = next((row for row in records if row['name'] == name), None)
    if previous:
        assert previous['command'] == list(map(str,command)) and previous['exit_code'] == expected_exit
        assert hashlib.sha256(path.read_bytes()).hexdigest() == previous['output_sha256']
        return path
    command = list(map(str, command))
    print('START '+name, flush=True)
    with path.open('wb') as out, (V/(name+'-stderr.log')).open('wb') as err:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=out, stderr=err, timeout=300)
    if result.returncode != expected_exit:
        raise RuntimeError(name+': '+(V/(name+'-stderr.log')).read_text()[-5000:])
    records.append(dict(name=name, command=command, exit_code=result.returncode, binary=binary,
                        output_sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    queue.write_text(json.dumps(records, indent=2)+'\n')
    print('DONE '+name, flush=True)
    return path


for label in labels:
    exe = E/'artifacts'/label/'build/Core'
    if phase != 'isolated':
        run('owning-'+label, [exe/'dexkit_uncached_descriptor_checks',
            BASE/'followup/fixtures/dense-descriptors/dense-descriptors.apk',
            BASE/'raw-metadata/symbol-fixture/symbols.apk'])
    if phase != 'normal':
        for fixture, oracle in [('symbol-fixture', 'symbol'), ('overload-fixture', 'overload')]:
            run('symbols-'+fixture+'-'+label, [exe/'dexkit_symbol_checks', BASE/'raw-metadata'/fixture/'symbols.apk'])
            result = run('symbol-dump-'+fixture+'-'+label,
                [exe/'dexkit_symbol_checks', '--dump', BASE/'raw-metadata'/fixture/'symbols.apk'], True)
            assert result.read_bytes() == gzip.decompress((ROOT/'tools/qaux-benchmark/evidence/raw-metadata'/(oracle+'-oracle.bin.gz')).read_bytes())
        run('dense-'+label, [exe/'dexkit_symbol_checks', '--dense', BASE/'followup/fixtures/dense-descriptors/dense-descriptors.apk'])
    if phase != 'normal':
        result = run('contracts-'+label, [exe/'dexkit_caller_checks', '--dump',
            BASE/'compact-callers-v2/fixtures/contracts/callers.apk'], True)
        assert result.read_bytes() == (BASE/'compact-callers-v2/validation/contracts-control-trace.bin').read_bytes()
        run('contracts-oracle-'+label, ['python3', ROOT/'tools/qaux-benchmark/validate_caller_fixture.py',
            '--manifest', BASE/'compact-callers-v2/fixtures/contracts/manifest.json', '--dump', result,
            '--log', V/('contracts-'+label+'-stderr.log')])
        for fixture, frozen in [('relations', BASE/'compact-callers-v2/validation/relations-control-trace.bin'),
                                ('field-adverse', BASE/'next-round/formal/compact-fields-control-trace-v1-field-adverse.bin')]:
            result = run(fixture+'-'+label, [exe/'dexkit_relation_checks', '--dump',
                BASE/'followup/fixtures'/fixture/'relations.apk'], True)
            assert result.read_bytes() == frozen.read_bytes()
        for size in ['tiny', 'mixed', 'giant']:
            result = run('invoke-'+size+'-'+label, [exe/'dexkit_invocation_checks', '--dump',
                BASE/'followup/fixtures'/('invoke-'+size)/'invocations.apk'], True)
            assert result.read_bytes() == gzip.decompress((ROOT/'tools/qaux-benchmark/evidence/followup/single'/('invoke-'+size+'-29-case-oracle.bin.gz')).read_bytes())
    if True:
        for component in ['index_types', 'caller_index', 'candidate_runtime', 'candidate_noexceptions', 'candidate_pipeline', 'inverted_string']:
            run(component+'-'+label, [exe/('dexkit_'+component+'_checks')])
    for workers in [1,4]:
        path = run('concurrent-'+str(workers)+'-'+label, [exe/'dexkit_descriptor_concurrent_workload',
            BASE/'raw-metadata/symbol-fixture/symbols.apk', '2000', workers])
        report = json.loads(next(line[9:] for line in path.read_text().splitlines() if line.startswith('WORKLOAD ')))
        assert report['calling_threads'] == workers and report['returned'] == 2001*workers*2
        assert report['lifecycle_ns'] > sum(report[k] for k in ['create_ns','setup_ns','first_ns','repeated_ns','close_ns'])

if phase != 'normal':
    for fixture in ['small', 'short', 'long', 'unresolved']:
        command = ['python3', ROOT/'tools/qaux-benchmark/validate_field_checks.py', '--fixture',
            BASE/'next-round/fixtures'/('field-'+fixture+'-v1'), '--output', V/('fields-'+phase+'-'+fixture)]
        for label in labels:
            command += ['--variant', label, E/'artifacts'/label]
        run('fields-'+phase+'-'+fixture, command)
        for label in labels:
            assert (V/('fields-'+phase+'-'+fixture)/label/'oracle.bin').read_bytes() == (BASE/'next-round/formal'/('compact-fields-'+fixture+'-validation-v1')/'control/oracle.bin').read_bytes()
(V/('complete-'+phase+'.json')).write_text(json.dumps(dict(phase=phase, commands=len(records),
    checks_sha256=hashlib.sha256(queue.read_bytes()).hexdigest(),
    validator_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()), indent=2)+'\n')
print('COMPLETE '+phase+' '+str(len(records))+' driver commands', flush=True)
