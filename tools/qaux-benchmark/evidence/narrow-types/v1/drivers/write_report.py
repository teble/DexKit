from pathlib import Path
import json

ROOT=Path('/Users/teble/project/android/DexKit-qaux-benchmark')
E=Path(__file__).resolve().parent
result=json.loads((E/'measurements/summary.json').read_text())
observations=result['observations']
gradle=json.loads((E/'validation/gradle/validation.json').read_text())
census=json.loads((E/'validation/census-summary.json').read_text())

def change(case,key,interval=True):
    parts=[]
    for phase in ['main','confirm']:
        m=case['phases'][phase][key]
        value='%+.2f%%'%m['median_change_percent']
        if interval:
            value+=' [%+.2f, %+.2f]'%tuple(m['bootstrap95_percent'])
        parts.append(value)
    return ' / '.join(parts)

def absolute(case,key,scale=1):
    return ' / '.join('%.2f -> %.2f'%(case['phases'][p][key]['variants']['control']['median']/scale,
        case['phases'][p][key]['variants']['narrow']['median']/scale) for p in ['main','confirm'])

assert census['saved_bytes']==66208612
assert result['samples']==240 and gradle['total_tests']==71 and gradle['skipped']==0
assert not any(c['support']['lifecycle_ms']=='higher_in_both' for c in observations.values())
assert {name for name,c in observations.items() if c['support']['lifecycle_ms']=='lower_in_both'}=={'mixed-invoke-match','qq-p1-w1'}
qq={name:case for name,case in observations.items() if case['kind']=='qq'}
assert all(c['support']['peak_footprint_bytes']=='lower_in_both' for c in qq.values())
lines=['''# Narrow native index types: completed results

The compatible type-only candidate reduces the measured QQ persistent index
capacity by **63.14 MiB**, without changing vector growth or element capacity.
Across four QQ workflows, physical process peak falls **4.02% to 4.77%** in the
main and confirmation batches. Preserve 32-bit method identities and keep the
new experiment OFF by default. Add `-PexperimentNarrowTypes=ON` to the previously
retained build arguments to select this candidate.

No measured complete-lifecycle regression has positive 95% intervals in both
batches. This finite sample does not establish universal performance equality.
Only mixed invoke matching and QQ single-thread/single-pass show a lifecycle
improvement supported in both batches. Other lifecycle comparisons remain
unresolved; in particular, QQ eleven-pass changes direction between batches.

## Final implementation and input domain

The fixed production source is
`c537b230c50bf08ac5a5e306d577ec0a6cb6c065`. `NARROW_TYPES` changes storage widths,
with checked conversion/addition during construction. Original row order,
duplicate occurrences, published views and vector growth remain unchanged.

| Stored value | OFF on 64-bit | ON | 32-bit target note |
| --- | ---: | ---: | --- |
| Compact offsets and caller count/cursor entries | 8 B | 4 B | Already 4 B when OFF |
| Invoke instruction operand | 4 B | 2 B | Same reduction |
| Class-definition index | 4 B | 2 B | Same reduction |
| Method identity | 4 B | 4 B | Preserved |
| Caller / field reader-writer record | 8 B | 8 B | Preserved |
| String ID and row length | 4 B | 4 B | Jumbo strings and long rows preserved |

Method definitions, class-method lists, pending identities, callers, field
readers/writers and resolved cross-DEX targets remain u32. There is no new
method-table limit. The first fully narrow prototype imposed such a limit and
was retired after a real DEX demonstrated the compatibility loss and the user
selected the compatible alternative.

The remaining declared limits are u32 cumulative element offsets/counts and at
most 65536 class-definition entries. An unrepresentable value fails an always
enabled check before narrowing or prefix payload allocation. These are element
counts, not a universal four-GiB byte limit. This is not a claim to preserve
arbitrary malformed input previously tolerated by the Reader. Native consumers
must use matching compile definitions; managed IDs and serialization stay wide.

## Actual retained-capacity attribution

The census is taken immediately before close in the same QQ eleven-pass
workflow, using matching diagnostic builds. It reports logical allocated
capacity of the listed persistent structures, separately from process memory.

| Component | Saving |
| --- | ---: |
| Using-string offset directory | 9.9183 MiB |
| Invoke offset directory | 9.9183 MiB |
| Caller prefix directory | 9.9185 MiB |
| Invoke element capacity | 32.0625 MiB |
| Class-definition indexes | 1.3238 MiB |
| **Total** | **66,208,612 B / 63.1415 MiB** |

Invoke payload capacity is 67,239,936 -> 33,619,968 bytes. Caller payload remains
87,490,896 bytes. String payload remains 11,436,032 bytes. Ordered caller hashes,
member rows, growth counts and element counts/capacities match; only the element
width changes the invocation movement/overlap byte totals. Temporary caller
counts/cursors also narrow, but their released storage is not included in this
persistent saving. The full census is in
[census-summary.json](evidence/narrow-types/v1/validation/census-summary.json).

## Fixed comparison and interpretation

Both normal artifacts use the same fixed production source, host compiler and
retained Small14 + RAW_DESCRIPTOR_LOOKUP + UNCACHED_DESCRIPTORS combination.
Only NARROW_TYPES differs; diagnostics and internal metrics are OFF. The control
is this existing optimization combination, not unmodified upstream master.

All **20 sweeps / 240 fresh processes** completed. Each case has six balanced
AB/BA pairs in a main batch and six more in an independently seeded confirmation.
Frozen input hashes reconciled before and after timing. All successful samples
are retained; no builds, correctness checks, profiling, compression or tuning
ran during measurement. Twelve native preflights are excluded from timing.

Values below are paired median percentage changes, main / confirmation;
negative favors narrow. Brackets are exploratory bootstrap 95% intervals from
six pairs, without multiple-comparison correction. Absolute medians are shown
separately and need not reproduce a paired median percentage. An interval that
crosses zero is not proof of equality.

Lifecycle includes construction, setup, query/output handling, destruction and
close. OS file caches were not flushed. These are Apple M1/macOS host timings;
Android evidence is compilation/layout, not device speed or memory measurement.
The complete phases and metrics are retained in
[summary.json](evidence/narrow-types/v1/summary.json).

## QQ workflows

| Case | Lifecycle change and 95% intervals | Physical peak change and 95% intervals |
| --- | --- | --- |''']
for name,case in qq.items():
    lines.append('| '+name+' | '+change(case,'lifecycle_ms')+' | '+change(case,'peak_footprint_bytes')+' |')
lines+=['','| Case | Lifecycle ms, control -> narrow | Physical peak MiB, control -> narrow |','| --- | --- | --- |']
for name,case in qq.items():
    lines.append('| '+name+' | '+absolute(case,'lifecycle_ms')+' | '+absolute(case,'peak_footprint_bytes',2**20)+' |')
lines+=['''
Single-thread/single-pass lifecycle improves 1.26% / 1.05%, with both intervals
below zero. Eleven-pass lifecycle changes -0.60% / +0.39%; the repeated API stage
also lacks support in both batches. The final-field workflow includes one actual
reader/writer metadata access before close, with independently frozen output.

## Native relation workloads

| Case | Lifecycle change and 95% intervals | Repeated API change | Physical peak change and 95% intervals |
| --- | --- | --- | --- |''']
for name,case in observations.items():
    if case['kind']!='native':
        continue
    lines.append('| '+name+' | '+change(case,'lifecycle_ms')+' | '+change(case,'repeated_api_ms',False)+' | '+change(case,'peak_footprint_bytes')+' |')
lines+=['''
Mixed invoke matching improves 3.17% / 2.69% over the whole lifecycle and
3.14% / 2.56% in its repeated stage; both intervals support each result. The
late-caller case has favorable point estimates but an unresolved main interval.
Cold four-worker caller matching has small positive timing estimates, with
no repeated lifecycle support. Its confirmation repeated stage is slightly
positive (+0.54%); the main repeated-stage interval includes zero.

Large output cases do not show a repeatable physical peak reduction. Giant
invoke output peaks at approximately 66 MiB and full-cache caller output at
136 MiB in both variants. Temporary result/serialization storage can dominate
these cases even though invocation element capacity shrinks. Their observed
positive samples remain included, rather than being removed as outliers.

## Verification and review

- 71 JVM tests passed, zero skipped; required Core, JAR and test tasks passed.
- Android release AAR built for arm64-v8a, armeabi-v7a, x86 and x86_64. Actual
  production compiler/prefab configurations plus host confirm the layout table;
  boundary components also compile with all five configurations.
- Normal, diagnostic, all-OFF, independent NARROW-only, ASan/UBSan and compact/
  packed-field combinations passed the relevant full ordered-output oracles.
  The 158 validation-driver invocations include batched semantic checks; they
  are not presented as 158 independent test cases. Leak detection was disabled
  in the host sanitizer configuration; address/undefined checks were enabled.
- Real DEX tables exercise IDs 65535 and 65536, 70000 invocation records,
  70004 decoded opcodes, jumbo string IDs, public class members and high caller/
  field reader/writer source identities. Both inputs pass six configurations.
- A further cross-DEX fixture checks local operand 0 resolving to remote method
  65536, alias/definition Bean output, physical reverse rows and class members
  before and after full cache; all six configurations pass.
- Multirow cumulative prefix overflow is rejected before large payload
  allocation. Public output order, duplicates and late/full span lifetimes
  retain their original checks.
- QQ complete frozen results pass one and eleven passes per workflow, one and
  four workers, including the final field reverse oracle. Documentation install
  and site build pass.
- [Source review and dispositions](NARROW-TYPES-REVIEW.md) records both Pro
  reviews, the compatibility decision and the corrected 32-bit component test.
  The final fixed-source review found no confirmed new production blocker.

One final long-field dump failed its output-write assertion while the disk was
full. Lossless compression of this task's retired prototype outputs freed space;
the unchanged final executable and input then passed. Failed/retry evidence is
retained. Initial supplemental-checker mistakes (a nonexistent scalar getter
overload and assuming caller alias redirection) were corrected against the
existing APIs/control, without altering production behavior.

## Build and evidence

The final tested Android artifact uses the existing retained 16-flag combination
plus `experimentNarrowTypes=ON`, effective Android Release `-Oz` and full LTO.
The exact Gradle command and verified per-ABI definitions are in
[validation.json](evidence/narrow-types/v1/validation/gradle/validation.json).
Keep the other build arguments from that command when reproducing these results;
enabling NARROW_TYPES alone is also tested but is a different comparison.
''']
lines.append('AAR SHA256: `'+gradle['aar_sha256']+'`.')
lines.append('')
lines.append('Frozen harness/plan commit: `'+result['harness_commit']+'`.')
lines.append('Frozen plan SHA256: `'+result['frozen_plan_sha256']+'`.')
lines.append('')
lines.append('Generated fixtures, drivers, artifacts manifests, complete samples/summaries and\ncompressed raw process/validation evidence are under\n[evidence/narrow-types/v1](evidence/narrow-types/v1/).')
lines.append('')
(ROOT/'tools/qaux-benchmark/NARROW-TYPES-RESULTS.md').write_text('\n'.join(lines))
print('Wrote completed results with all ten comparison cases')
