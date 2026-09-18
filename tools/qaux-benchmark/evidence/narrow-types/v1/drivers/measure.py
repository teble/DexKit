"""Freeze one type-only comparison: six relation cases and four QQ workflows."""
import datetime
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path('/Users/teble/project/android/DexKit-qaux-benchmark')
E = Path(__file__).resolve().parent
BASE = E.parent.parent
OUT = E/'measurements'
JDK = Path('/Users/teble/Library/Java/JavaVirtualMachines/jbr-17.0.6/Contents/Home')
ENGINE = 'c537b230c50bf08ac5a5e306d577ec0a6cb6c065'
LABELS = ['control', 'narrow']
ARTIFACTS = {label:E/'artifacts'/label for label in LABELS}
CASES = [
    ('mixed-invoke-match', 'mixed', 'invoke-match', 32),
    ('mixed-invoke-multiple', 'mixed', 'invoke-multiple', 32),
    ('giant-invoke-output', 'giant', 'invoke-output', 4),
    ('mixed-caller-late-w1', 'mixed', 'caller-match-late-w1', 32),
    ('mixed-caller-cold-w4', 'mixed', 'caller-multiple-cold-w4', 32),
    ('giant-caller-full-w4', 'giant', 'caller-output-full-w4', 4),
]
FIXTURES = {name:BASE/'followup/fixtures'/('invoke-'+name)/'invocations.apk' for name in ['mixed','giant']}
TAIL = ['--final-field-rw', 'Lcom/tencent/mobileqq/data/MessageRecord;->msg:Ljava/lang/String;',
        '--field-rw-expected', BASE/'followup/formal/field-tail-oracle-best5/field-rw-expected.json']

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for data in iter(lambda:stream.read(1024*1024), b''):
            h.update(data)
    return h.hexdigest()

def executable(label):
    return ARTIFACTS[label]/'build/Core/dexkit_invocation_workload'

def plan():
    result = []
    for phase,seed in [('main',2026091830),('confirm',2026091840)]:
        for name,fixture,mode,repeats in CASES:
            seed += 1
            result.append(dict(name=phase+'-'+name,phase=phase,kind='native',case=name,
                fixture=str(FIXTURES[fixture]),mode=mode,repeats=repeats,pairs=6,seed=seed))
        for passes,workers,tail in [(1,4,False),(11,4,False),(1,1,False),(1,4,True)]:
            seed += 1
            name=('qq-tail' if tail else 'qq')+'-p'+str(passes)+'-w'+str(workers)
            result.append(dict(name=phase+'-'+name,phase=phase,case=name,kind='qq',passes=passes,
                               threads=workers,tail=tail,pairs=6,seed=seed))
    assert len(result)==20 and len({x['name'] for x in result})==20
    return result

def command(item):
    if item['kind']=='native':
        cmd=[sys.executable,ROOT/'tools/qaux-benchmark/workload_sweep.py','--fixture',item['fixture'],
             '--output',OUT/item['name'],'--mode',item['mode'],'--repeats',item['repeats']]
        for label in LABELS:
            cmd+=['--variant',label,ARTIFACTS[label]]
    else:
        cmd=[sys.executable,ROOT/'tools/qaux-benchmark/sweep.py','--dexkit-root',ROOT,
             '--corpus',BASE/'qaux-extracted','--apk',BASE/'qq-9.3.55.apk','--java-home',JDK,
             '--memory-probe',BASE/'artifacts/probe-v2/libqauxbench_probe.dylib','--output',OUT/item['name'],
             '--passes',item['passes'],'--threads',item['threads'],'--profile','all']
        if item['tail']:
            cmd+=TAIL
        for label in LABELS:
            verified='verify-'+label+('-tail' if item['tail'] else '')+'-w'+str(item['threads'])
            cmd+=['--variant',label,ARTIFACTS[label]/'libdexkit.dylib',E/'validation'/verified]
    return list(map(str,cmd+['--pairs',item['pairs'],'--seed',item['seed']]))

action=sys.argv[1]
assert action in ['smoke','freeze','run']
if action=='smoke':
    records=[]
    destination=E/'validation/workload-smoke'
    destination.mkdir()
    for name,fixture,mode,repeats in CASES:
        identities=[]
        for label in LABELS:
            cmd=list(map(str,[executable(label),FIXTURES[fixture],mode,2]))
            result=subprocess.run(cmd,cwd=ROOT,capture_output=True,timeout=300,check=True)
            (destination/(name+'-'+label+'.log')).write_bytes(result.stdout+result.stderr)
            report=json.loads(next(line[9:] for line in result.stdout.decode().splitlines() if line.startswith('WORKLOAD ')))
            identities.append({key:report[key] for key in ['mode','repeats','checksum','returned']})
            records.append(dict(case=name,label=label,command=cmd,report=report))
        assert identities[0]==identities[1]
    (destination/'summary.json').write_text(json.dumps(records,indent=2)+'\n')
    print('PASS 12 native workload preflights; excluded from timing')
    sys.exit(0)

OUT.mkdir(exist_ok=True)
frozen_path=OUT/'frozen-plan.json'
items=plan()
if action=='freeze':
    assert not frozen_path.exists()
    assert not subprocess.check_output(['git','-C',ROOT,'status','--porcelain'],text=True).strip()
    paths=list(E.glob('*.py'))+[BASE/'qq-9.3.55.apk',JDK/'bin/java',JDK/'bin/javac',
        BASE/'artifacts/probe-v2/libqauxbench_probe.dylib',*FIXTURES.values(),TAIL[-1]]
    paths+=[ROOT/'tools/qaux-benchmark'/name for name in ['workload_sweep.py','sweep.py','run.py','QueryReplay.java',
        'invocation_workload.cpp','invocation_queries.h','NARROW-TYPES-EXECUTION.md','NARROW-TYPES-REVIEW.md','baseline/expected.json']]
    paths+=list((BASE/'qaux-extracted').glob('*.json'))
    for phase in ['normal','trace','isolated','sanitize','combination']:
        complete=E/'validation'/('complete-'+phase+'.json')
        checks=E/'validation'/('checks-'+phase+'.json')
        assert json.loads(complete.read_text())['checks_sha256']==sha(checks)
        paths+=[complete,checks]
    for phase in ['normal','isolated','sanitize','combination']:
        path=E/'validation'/('width-api-'+phase+'.json')
        proof=json.loads(path.read_text())
        assert all(r['exit_code']==0 and r['expected_exit']==0 and r['engine_commit']==ENGINE for r in proof)
        paths.append(path)
    for name in ['gradle/validation.json','docs/validation.json','qq-verification.json','census-qq.json',
                 'census-summary.json','layout/summary.json','workload-smoke/summary.json','remote-width.json']:
        path=E/'validation'/name
        assert path.is_file()
        paths.append(path)
    gradle=json.loads((E/'validation/gradle/validation.json').read_text())
    assert gradle['total_tests']==71 and gradle['skipped']==0 and len(gradle['abis'])==4
    assert all(r['exit_code']==0 for r in json.loads((E/'validation/docs/validation.json').read_text()))
    assert len(json.loads((E/'validation/layout/summary.json').read_text()))==5
    assert len(json.loads((E/'validation/workload-smoke/summary.json').read_text()))==12
    assert len(json.loads((E/'validation/remote-width.json').read_text()))==6
    paths += [E/'fixtures/remote-wide/widths.apk',E/'fixtures/remote-wide/manifest.json',E/'remote_width_api_checks.cpp',
              E.parent/'make_width_fixtures.py',E.parent/'width_api_checks.cpp']
    paths += [E.parent/'fixtures'/shape/name for shape in ['limit','wide'] for name in ['manifest.json','widths.apk']]
    manifests={}
    for label in LABELS:
        artifact=ARTIFACTS[label]
        manifest=json.loads((artifact/'artifact.json').read_text())
        manifests[label]=manifest
        assert manifest['engine_commit']==ENGINE and not manifest['diagnostics'] and not manifest['untracked_source_sha256']
        assert (artifact/'source.patch').read_text()=='\n' and manifest['native_sha256']==sha(artifact/'libdexkit.dylib')
        paths+=[artifact/'artifact.json',artifact/'libdexkit.dylib',artifact/'workload-sources.json',executable(label)]
        options=manifest['cmake_options']
        assert options['DEXKIT_EXPERIMENT_NARROW_TYPES']==('ON' if label=='narrow' else 'OFF')
        for key in ['DEXKIT_ENABLE_INTERNAL_METRICS','DEXKIT_ENABLE_INTERNAL_METRICS_API','DEXKIT_BENCHMARK_FIELD_TRACE']:
            assert options[key]=='OFF'
        for workers,tail in [(1,False),(4,False),(4,True)]:
            path=E/'validation'/('verify-'+label+('-tail' if tail else '')+'-w'+str(workers))/'run-metadata.json'
            metadata=json.loads(path.read_text())
            assert metadata['exit_code']==0 and metadata['baseline_results_equal'] is True
            assert metadata['native_sha256']==manifest['native_sha256']
            if tail:
                assert metadata['final_field_rw_results_equal'] is True
            paths+=[path]+list(map(Path,metadata['jars_sha256']))
    normalized=[{k:v for k,v in m['cmake_options'].items() if k!='DEXKIT_EXPERIMENT_NARROW_TYPES'} for m in manifests.values()]
    assert normalized[0]==normalized[1]
    assert manifests['control']['compiler']==manifests['narrow']['compiler']
    frozen=dict(frozen_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),platform=platform.platform(),
        worktree_head=subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip(),engine_commit=ENGINE,
        policy='20 sweeps / 240 fresh processes, main and independently seeded confirmation, six balanced AB/BA pairs. '
            'Only NARROW_TYPES differs, on the retained Small14 + RAW_DESCRIPTOR_LOOKUP + UNCACHED configuration. '
            'Retain all successful samples and adverse results; no tuning, compilation, checks, profiling or compression during timing. '
            'File caches are not flushed. Lifecycle includes construction, queries, output destruction and close. '
            'Directory and payload census is separate from physical process peak. Host timing does not establish Android device speed.',
        files={str(p):sha(p) for p in sorted(set(paths))},native_manifests=manifests,sweeps=[dict(x,command=command(x)) for x in items])
    frozen_path.write_text(json.dumps(frozen,indent=2)+'\n')
    print('FROZEN '+sha(frozen_path)+' 20 sweeps / 240 processes',flush=True)
    sys.exit(0)

frozen=json.loads(frozen_path.read_text())
assert frozen['sweeps']==[dict(x,command=command(x)) for x in items]
for path,expected in frozen['files'].items():
    assert sha(path)==expected,'Frozen input changed: '+path
queue=OUT/'completed.json'
completed=json.loads(queue.read_text()) if queue.exists() else []
for item in frozen['sweeps']:
    name=item['name']
    old=next((r for r in completed if r['name']==name),None)
    if old:
        assert old['summary_sha256']==sha(OUT/name/'summary.json')
        continue
    print('START '+name,flush=True)
    with (OUT/(name+'-launcher.log')).open('w') as log:
        subprocess.run(item['command'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
    summary=json.loads((OUT/name/'summary.json').read_text())
    completed.append(dict(item,summary_sha256=sha(OUT/name/'summary.json')))
    queue.write_text(json.dumps(completed,indent=2)+'\n')
    print('DONE '+name+' lifecycle=%+.2f%%'%summary['metrics']['lifecycle_ms']['median_change_percent'],flush=True)
for path,expected in frozen['files'].items():
    assert sha(path)==expected,'Frozen input changed after timing: '+path
print('COMPLETE 20 sweeps / 240 fresh processes; frozen inputs unchanged',flush=True)
