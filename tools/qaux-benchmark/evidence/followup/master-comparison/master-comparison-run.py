import json, os, pathlib, subprocess, sys

W = pathlib.Path('/Users/teble/project/android/DexKit-qaux-benchmark')
D = pathlib.Path('/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/followup')
P = D.parent
F = D / 'formal'
J = '/Users/teble/Library/Java/JavaVirtualMachines/jbr-17.0.6/Contents/Home'
E = os.environ.copy()
E.update(JAVA_HOME=J, PATH=J + '/bin:' + E['PATH'])
common = ['--dexkit-root', W, '--corpus', P / 'qaux-extracted', '--apk', P / 'qq-9.3.55.apk',
          '--java-home', J, '--profile', 'all', '--threads', 4,
          '--memory-probe', P / 'artifacts/probe-v2/libqauxbench_probe.dylib']
tail = ['--final-field-rw', 'Lcom/tencent/mobileqq/data/MessageRecord;->msg:Ljava/lang/String;',
        '--field-rw-expected', F / 'field-tail-oracle-best5/field-rw-expected.json']
variants = [('master-1d936bd', P / 'artifacts/baseline-rebuilt/libdexkit.dylib'),
            ('current-combo', D / 'artifacts/combo-v1/libdexkit.dylib')]

def run(stage, cmd):
    print('Starting', stage, flush=True)
    (D / 'master-comparison-progress.json').write_text(json.dumps({'stage': stage, 'state': 'running'}))
    with (F / (stage + '.log')).open('w') as log:
        result = subprocess.run(list(map(str, cmd)), cwd=W, env=E, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((F / (stage + '.log')).read_text()[-6000:])
        raise SystemExit(result.returncode)
    print('Completed', stage, flush=True)

for scenario, extra in [('normal', []), ('tail', tail)]:
    for label, library in variants:
        run('verify-master-' + scenario + '-' + label,
            [sys.executable, W / 'tools/qaux-benchmark/run.py', *common, *extra,
             '--mode', 'verify', '--passes', 11, '--native-library', library,
             '--output', F / ('verify-master-' + scenario + '-' + label)])

for batch, seed in [('main', 2026091537), ('confirm', 2026091538)]:
    for scenario, extra in [('normal', []), ('tail', tail)]:
        for passes in [1, 11]:
            stage = 'qq-master-' + scenario + '-' + batch + '-p' + str(passes)
            cmd = [sys.executable, W / 'tools/qaux-benchmark/sweep.py', *common, *extra,
                   '--pairs', 6, '--seed', seed, '--passes', passes, '--output', F / stage]
            for label, library in variants:
                cmd += ['--variant', label, library, F / ('verify-master-' + scenario + '-' + label)]
            run(stage, cmd)

(D / 'master-comparison-progress.json').write_text(json.dumps({'stage': 'complete', 'state': 'complete'}))
