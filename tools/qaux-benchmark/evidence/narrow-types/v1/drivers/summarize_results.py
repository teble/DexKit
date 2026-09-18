import hashlib
import json
from pathlib import Path

E=Path(__file__).resolve().parent
M=E/'measurements'
frozen=json.loads((M/'frozen-plan.json').read_text())
completed=json.loads((M/'completed.json').read_text())
assert len(completed)==20
observations={}
samples=0
for item in completed:
    source=M/item['name']/'summary.json'
    assert hashlib.sha256(source.read_bytes()).hexdigest()==item['summary_sha256']
    report=json.loads(source.read_text())
    rows=json.loads((M/item['name']/'samples.json').read_text())
    assert len(rows)==12 and report['pairs']==6
    samples+=len(rows)
    case=observations.setdefault(item['case'],dict(kind=item['kind'],phases={}))
    case['phases'][item['phase']]=report['metrics']
for case in observations.values():
    assert set(case['phases'])=={'main','confirm'}
    metrics=set(case['phases']['main'])&set(case['phases']['confirm'])
    case['support']={}
    for key in sorted(metrics):
        intervals=[case['phases'][p][key]['bootstrap95_percent'] for p in ['main','confirm']]
        case['support'][key]=('lower_in_both' if all(hi<0 for lo,hi in intervals) else
            'higher_in_both' if all(lo>0 for lo,hi in intervals) else 'not_resolved_in_both')
assert samples==240
result=dict(engine_commit=frozen['engine_commit'],harness_commit=frozen['worktree_head'],
    frozen_plan_sha256=hashlib.sha256((M/'frozen-plan.json').read_bytes()).hexdigest(),
    samples=samples,observations=observations)
(M/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
for name,case in observations.items():
    print(name,case['support']['lifecycle_ms'])
    for key in ['lifecycle_ms','peak_footprint_bytes','repeated_api_ms']:
        if key not in case['support']:
            continue
        values=[]
        for p in ['main','confirm']:
            m=case['phases'][p][key]
            values.append((round(m['median_change_percent'],3),[round(x,3) for x in m['bootstrap95_percent']],
                {label:round(m['variants'][label]['median']/(2**20 if key.endswith('_bytes') else 1),3) for label in ['control','narrow']}))
        print(' ',key,values)
