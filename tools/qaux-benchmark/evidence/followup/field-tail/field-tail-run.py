import json, os, pathlib, subprocess, sys

W = pathlib.Path('/Users/teble/project/android/DexKit-qaux-benchmark')
D = pathlib.Path('/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/followup')
P = D.parent
J = '/Users/teble/Library/Java/JavaVirtualMachines/jbr-17.0.6/Contents/Home'
E = os.environ.copy()
E.update(JAVA_HOME=J, PATH=J + '/bin:' + E['PATH'])
F = D / 'formal'
field = 'Lcom/tencent/mobileqq/data/MessageRecord;->msg:Ljava/lang/String;'
oracle = F / 'field-tail-oracle-best5/field-rw-expected.json'
common = ['--dexkit-root', W, '--corpus', P / 'qaux-extracted', '--apk', P / 'qq-9.3.55.apk',
          '--java-home', J, '--profile', 'all', '--threads', '4',
          '--memory-probe', P / 'artifacts/probe-v2/libqauxbench_probe.dylib',
          '--final-field-rw', field, '--field-rw-expected', oracle]

def run(stage, cmd):
    print('Starting', stage, flush=True)
    (D / 'field-tail-progress.json').write_text(json.dumps({'stage': stage, 'state': 'running'}))
    with (F / (stage + '.log')).open('w') as log:
        result = subprocess.run(list(map(str, cmd)), cwd=W, env=E, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((F / (stage + '.log')).read_text()[-6000:])
        raise SystemExit(result.returncode)
    print('Completed', stage, flush=True)

confirm = '--confirm' in sys.argv
labels = ['best5-single-control', 'field-split-timers-v3', 'combo-v1']
if not confirm:
    for label in labels + ['best5-final-checks', 'combo-checks-v1']:
        run('verify-field-tail-' + label, [sys.executable, W / 'tools/qaux-benchmark/run.py', *common,
            '--mode', 'verify', '--passes', 1 if 'checks' in label else 11,
            '--native-library', D / 'artifacts' / label / 'libdexkit.dylib',
            '--output', F / ('verify-field-tail-' + label)])

for candidate, short in [('field-split-timers-v3', 'field'), ('combo-v1', 'combo')]:
    for passes in [1, 11]:
        stage = 'qq-field-tail-' + short + ('-confirm' if confirm else '-main') + '-p' + str(passes)
        cmd = [sys.executable, W / 'tools/qaux-benchmark/sweep.py', *common,
               '--pairs', 6, '--seed', 2026091536 if confirm else 2026091535,
               '--passes', passes, '--output', F / stage]
        for label in ['best5-single-control', candidate]:
            cmd += ['--variant', label, D / 'artifacts' / label / 'libdexkit.dylib',
                    F / ('verify-field-tail-' + label)]
        run(stage, cmd)

(D / 'field-tail-progress.json').write_text(json.dumps({'stage': 'confirm-complete' if confirm else 'main-complete', 'state': 'complete'}))
