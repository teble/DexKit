import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path('/Users/teble/project/android/DexKit-qaux-benchmark')
E = Path(__file__).resolve().parent
BASE = E.parent.parent
V = E/'validation'
JDK = '/Users/teble/Library/Java/JavaVirtualMachines/jbr-17.0.6/Contents/Home'
records = []
for label, tail in [('control-trace', False), ('narrow-trace', False), ('control', True), ('narrow', True)]:
    name = 'verify-'+label+('-tail' if tail else '-census')+'-w4'
    command = ['python3', ROOT/'tools/qaux-benchmark/run.py', '--dexkit-root', ROOT,
        '--corpus', BASE/'qaux-extracted', '--apk', BASE/'qq-9.3.55.apk', '--java-home', JDK,
        '--memory-probe', BASE/'artifacts/probe-v2/libqauxbench_probe.dylib', '--output', V/name,
        '--native-library', E/'artifacts'/label/'libdexkit.dylib', '--passes', '11', '--threads', '4',
        '--profile', 'all', '--mode', 'verify']
    if tail:
        command += ['--final-field-rw', 'Lcom/tencent/mobileqq/data/MessageRecord;->msg:Ljava/lang/String;',
            '--field-rw-expected', BASE/'followup/formal/field-tail-oracle-best5/field-rw-expected.json']
    command = list(map(str, command))
    print('START '+name, flush=True)
    with (V/(name+'-launcher.log')).open('w') as log:
        subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=300, check=True)
    metadata = json.loads((V/name/'run-metadata.json').read_text())
    assert metadata['baseline_results_equal'] is True
    if tail:
        assert metadata['final_field_rw_results_equal'] is True
    reports = {}
    if not tail:
        for line in (V/name/'run.log').read_text().splitlines():
            if line.startswith('BENCH_'):
                prefix, text = line.split(' ', 1)
                try:
                    value = json.loads(text)
                except json.JSONDecodeError:
                    continue
                if value.get('phase') == 'pre_close':
                    reports.setdefault(prefix, []).append(value)
    records.append(dict(label=label, tail=tail, command=command, reports=reports))
    (V/'census-qq.json').write_text(json.dumps(records, indent=2)+'\n')
    print('DONE '+name, flush=True)

a,b = [r['reports'] for r in records if not r['tail']]
for category in ['BENCH_CALLER_ROWS','BENCH_MEMBER_ROWS']:
    assert sorted(a[category], key=lambda x:(x['dex'], x.get('kind',''))) == sorted(b[category], key=lambda x:(x['dex'], x.get('kind',''))), category
for category, scale in [('BENCH_GROWTH', 1), ('BENCH_INVOKE_GROWTH', 2)]:
    left,right = [sorted(r[category], key=lambda x:x['dex']) for r in [a,b]]
    assert len(left) == len(right)
    for x,y in zip(left,right):
        for key in x:
            assert x[key] == (y[key]*scale if key in ['moved_id_bytes', 'peak_overlap_bytes'] else y[key]), (category, key, x,y)
left,right = [{x['category']:x for x in r['BENCH_CENSUS'] if 'category' in x} for r in [a,b]]
deltas = []
for key in left:
    assert set(left[key]) == set(right[key])
    for metric in ['entries','nonempty_or_ready','live_buffers']:
        assert left[key][metric] == right[key][metric], (key,metric)
    deltas.append(dict(category=key, before=left[key], after=right[key], saved_bytes=sum(left[key][m]-right[key][m] for m in ['index_bytes','payload_capacity_bytes'])))
summary = dict(categories=deltas, saved_bytes=sum(r['saved_bytes'] for r in deltas),
               logical_caller_rows_identical=True, member_rows_identical=True, growth_counts_and_element_capacities_identical=True)
(V/'census-summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print('SAVED '+str(summary['saved_bytes'])+' bytes', flush=True)
