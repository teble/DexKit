import json
from pathlib import Path
import subprocess

ROOT = Path('/Users/teble/project/android/DexKit-qaux-benchmark')
BASE = Path('/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55')
E = BASE/'narrow-types-v1/r3'
JDK = '/Users/teble/Library/Java/JavaVirtualMachines/jbr-17.0.6/Contents/Home'
labels = ['control','narrow']
plan = [(label, threads, 11 if threads == 4 else 1) for label in labels for threads in [4, 1]]
queue = E/'validation/qq-verification.json'
records = json.loads(queue.read_text()) if queue.exists() else []
for label, threads, passes in plan:
    name = 'verify-' + label + '-w' + str(threads)
    if any(row['name'] == name for row in records):
        continue
    command = ['python3', 'tools/qaux-benchmark/run.py', '--dexkit-root', ROOT,
               '--corpus', BASE/'qaux-extracted', '--apk', BASE/'qq-9.3.55.apk',
               '--java-home', JDK, '--memory-probe', BASE/'artifacts/probe-v2/libqauxbench_probe.dylib',
               '--output', E/'validation'/name, '--native-library', E/'artifacts'/label/'libdexkit.dylib',
               '--passes', str(passes), '--threads', str(threads), '--profile', 'all', '--mode', 'verify']
    command = list(map(str, command))
    print('START '+name, flush=True)
    with (E/'validation'/(name+'-launcher.log')).open('w') as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
    record = json.loads((E/'validation'/name/'run-metadata.json').read_text())
    assert record['baseline_results_equal'] is True
    records.append(dict(name=name, command=command, passed=True))
    queue.write_text(json.dumps(records, indent=2)+'\n')
    print('DONE '+name, flush=True)
