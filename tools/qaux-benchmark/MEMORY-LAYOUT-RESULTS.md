# Memory layout results

Measured normal engine: `62687fb6f0fef745f3614124650d45924293fd6c`. Diagnostic correction: `05e5a41`.
Both phases completed: 76 sweeps, 912 fresh processes, no removed measurements.
All frozen input hashes and per-sweep identities were reconciled after completion.

## Decision

Retain packed cross references and packed field-use rows as bounded memory
improvements on this experimental branch. Their combined QQ peak reduction
is 2.20% to 2.92% across the six main/confirmation configurations. Complete
workflow medians improve 1.02% to 3.98%, but several intervals cross zero;
do not call that a guaranteed time speedup. Cross-only small fixtures also
have a small adverse physical-peak case, retained in the table below.

Keep the node cache conditional on sparse descriptor workloads. QQ peak
falls 8.73% to 9.22%, while full-SSO lifecycle regresses 29.08% to 36.95% and
wide cached-text lookup regresses 310.10% to 316.33%. Sparse 1024-entry output
also has slower first/warm APIs despite a lower complete lifecycle/peak.
It is not the general replacement for the old dense descriptor cache.

All three together improve this QQ workload: peak falls 11.46% to 11.97%;
one-pass/four-worker lifecycle changes -5.73% and -5.50%. Eleven-pass medians
change -1.69% and -3.39%, with the main interval still crossing zero. This
combination inherits the node cache's adverse applicability boundary.

The user subsequently proposed sparse-to-dense conversion. That now takes
priority as a separate experiment with stable string ownership and a
sparse-only control; it does not retroactively change this frozen batch.
[Hybrid design and validation plan](HYBRID-DESCRIPTORS-EXECUTION.md).

The fixed baseline is Small14 (Small13 plus COMPACT_CALLERS). `cross` adds
PACKED_CROSS_INFO; `node` adds NODE_DESCRIPTORS; `field` adds PACKED_FIELD_USES.
`packed` adds cross+field; `combined` adds all three. New switches remain
default OFF. Each row compares its candidate directly with Small14; changes
from historical baselines are not added together.

Host: Apple M1. ProductName:		macOS; ProductVersion:		26.5.1; BuildVersion:		25F80.
Normal builds use Release -O3 with internal metrics and diagnostics OFF.
Builds and functional checks finished before timing. Each phase uses six
balanced AB/BA pairs per case, with independent seeds. Every QQ sample is a
fresh JVM, using the frozen 139-target/149-group extraction and ten serial
workflows, including their existing empty-result branches. OS file caches
were not flushed. Adapter compilation precedes the measured bridge lifecycle.

Percentages below are paired median changes, main / confirmation; negative
favors the candidate. All raw values and exploratory paired bootstrap 95%
intervals are in the linked evidence. An interval crossing zero is not a
proof of equivalence; large percentages on tiny close times need absolute
times. A one-pass workload has no repeated-API measurement.

## QQ complete workflows

| Candidate / passes / workers | Lifecycle change | Peak footprint change | Repeated APIs change |
| --- | ---: | ---: | ---: |
| cross-qq-p1-w4 | -0.60% / -0.51% | -0.85% / -1.22% | - |
| cross-qq-p11-w4 | -2.10% / -1.48% | -1.02% / -1.13% | -1.98% / -1.73% |
| cross-qq-p1-w1 | -2.34% / +0.02% | -1.23% / -1.38% | - |
| node-qq-p1-w4 | -2.89% / -1.67% | -9.03% / -9.22% | - |
| node-qq-p11-w4 | -1.31% / -0.45% | -9.18% / -8.73% | -0.30% / -0.17% |
| node-qq-p1-w1 | -0.20% / -0.28% | -8.94% / -9.16% | - |
| field-qq-p1-w4 | +1.97% / -2.49% | -1.21% / -1.25% | - |
| field-qq-p11-w4 | -0.02% / -1.20% | -1.66% / -1.35% | -0.12% / -1.35% |
| field-qq-p1-w1 | +1.46% / -1.37% | -1.37% / -1.52% | - |
| field-qq-tail-p1-w4 | -0.61% / -2.18% | -1.32% / -1.24% | - |
| packed-qq-p1-w4 | -2.80% / -3.98% | -2.36% / -2.36% | - |
| packed-qq-p11-w4 | -1.02% / -2.84% | -2.20% / -2.73% | -0.09% / -3.06% |
| packed-qq-p1-w1 | -1.82% / -1.48% | -2.92% / -2.41% | - |
| combined-qq-p1-w4 | -5.73% / -5.50% | -11.66% / -11.91% | - |
| combined-qq-p11-w4 | -1.69% / -3.39% | -11.97% / -11.73% | -0.60% / -1.19% |
| combined-qq-p1-w1 | -4.06% / -4.49% | -11.46% / -11.46% | - |

Lifecycle includes create, the required query/cache work, result handling and
close. Peak footprint includes JVM/native allocations and allocator overhead;
it is not a native-only or Android-device memory measurement. The late-field
case includes its final reader/writer build and getter before close.
Full ordered descriptors and flow were verified before timing; timed runs
check counts and flow to keep hashing/verbose verification outside the window.

## Native counterexamples

| Candidate / fixture / mode / repetitions | Lifecycle change | Peak footprint change | Repeated APIs change |
| --- | ---: | ---: | ---: |
| cross-adverse-field-full-first-r16 | +0.09% / +1.29% | +1.22% / +3.28% | +0.72% / +2.61% |
| cross-field-long-using-multiple-r16 | +0.12% / -1.20% | +0.58% / +0.63% | -0.44% / -1.05% |
| cross-invoke-giant-caller-match-cold-w4-r16 | -3.96% / -3.96% | -0.19% / +0.10% | -4.09% / -4.01% |
| cross-invoke-mixed-caller-output-full-w4-r16 | -0.03% / -0.23% | -0.22% / +0.12% | -0.14% / -0.34% |
| node-dense-output-sso-r2 | +36.04% / +36.95% | +12.79% / +12.85% | +28.17% / +16.63% |
| node-dense-output-sso-r16 | +29.08% / +29.48% | +11.36% / +9.87% | +26.82% / +26.86% |
| node-dense-output-sso-prefix-r16 | -3.21% / -5.13% | -23.54% / -23.42% | +8.97% / +10.45% |
| node-dense-output-sso-scattered-r16 | -0.56% / -1.89% | -23.55% / -23.53% | +6.56% / +9.24% |
| node-dense-output-sso-shard-r16 | -1.29% / -2.86% | -23.70% / -24.11% | +9.84% / +9.47% |
| node-symbol-output-r16 | +0.51% / +5.10% | -4.65% / -1.62% | -0.63% / +7.54% |
| node-symbol-lookup-r512 | +310.10% / +316.33% | +2.93% / +2.93% | +400.71% / +417.57% |
| node-overload-lookup-prefix-r256 | +45.36% / +48.83% | +1.95% / +2.31% | +47.40% / +50.24% |
| node-symbol-lookup-hot-r100000 | +29.91% / +29.68% | -5.57% / -5.19% | +32.15% / +31.79% |
| node-symbol-lookup-concurrent-w1-r20000 | +52.00% / +63.98% | -8.00% / -7.11% | +54.33% / +66.94% |
| node-symbol-lookup-concurrent-w4-r20000 | +4.35% / +16.76% | -6.46% / -7.26% | +4.31% / +16.86% |
| field-field-short-using-early-r16 | -6.33% / -1.88% | -4.23% / -0.18% | -7.05% / -1.04% |
| field-field-long-using-late-r16 | -2.37% / -2.31% | -26.32% / -25.98% | +0.19% / -2.40% |
| field-field-long-using-miss-r16 | -1.85% / -4.00% | -27.92% / -28.02% | -1.61% / -4.10% |
| field-field-long-using-multiple-r16 | -1.40% / -2.57% | -26.45% / -26.25% | -1.16% / -2.04% |
| field-field-long-using-output-r2 | -0.55% / +0.34% | -26.77% / -24.97% | +0.33% / -1.10% |
| field-adverse-field-late-r16 | +1.90% / +0.20% | +5.72% / -1.88% | +1.04% / +0.05% |
| field-adverse-field-full-first-r16 | -1.09% / -0.69% | -1.16% / -3.43% | -1.04% / -0.90% |

Native lifecycle includes setup, output serialization/destruction and bridge
close. SSO output retains the method result while requesting fields; both
variants use that same overlap. Concurrent modes each compare identical work
within A/B: four calling threads do four times the work of one. First time
includes barrier arrival; warm time covers a group of four APIs per thread.
Join, checksum containers and barriers finish before the lifecycle endpoint.

## Logical capacity and ownership

| QQ retained layout | Small14 MiB | Combined MiB | Saved MiB |
| --- | ---: | ---: | ---: |
| Cross-DEX identities | 46.840 | 31.226 | 15.613 |
| Field-use payload | 52.878 | 26.439 | 26.439 |
| Descriptor cache with symmetric owners | 129.079 | 0.467 | 128.612 |

The combined logical reduction is 170.664 MiB; it is not a measured peak reduction.
Field-use row directories remain 59.510 MiB with 1540690 buffers; packing changes
the payload and copied token size, not the row count or number of allocations.
QQ generates 1628 methods and 0 fields; 18051 calls include 16423 hits.
The node total includes 210904 fixed bytes, 30088 bucket bytes, 52096 node bytes and
196369 external character bytes. The old dense census is corrected by 2624 bytes
for omitted owner objects on this host only. Diagnostic counters are excluded.
The largest one-table old+new bucket overlap is 216 bytes; it is not a global peak.

Full 120000-entry SSO coverage reverses the descriptor layout result: old
dense 3962112 bytes, node 6204440 bytes (2.138 MiB more). Character bytes are zero
because all strings use SSO. Sparse savings do not imply dense savings.
Neither the requested layout census nor individual bucket overlap includes
all allocator metadata, every native allocation or a simultaneous process peak.

## Validation and source review

- 250 native driver commands: normal 70, trace/isolated 122, ASan/UBSan 30,
  packed-field plus COMPACT_FIELDS 28. Independent field manifests and frozen
  complete caller/relation/invocation/symbol/overload bytes agree.
- Stable SSO/long/empty views survive concurrent same-ID/same-shard/other-shard
  node growth. Normal non-diagnostic cache components and 38 workload smoke
  commands pass. The field encoder covers both directions for all 65536 IDs
  and actually aborts for out-of-domain inputs.
- 17 QQ verifications include six normal variants with one/four workers,
  trace census and late reader/writer oracles. All frozen results agree.
- 71 JVM tests pass with no skips; Android AAR builds pass for arm64-v8a,
  armeabi-v7a, x86 and x86_64. Normal/diagnostic layout probes agree for those
  four ABIs and macOS arm64.
- Host ASan/UBSan has leak detection disabled. Concurrent stress is not a
  proof of absence of all data races. No Android device timing is claimed.

[Pro source-review scope and local checks](MEMORY-LAYOUT-REVIEW.md) records
the fixed increments actually read. Pro ran no tests or measurements.
[Original Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3).

## Artifact size

| Normal library | Unstripped dylib bytes |
| --- | ---: |
| control | 1047904 |
| cross | 1047904 |
| node | 1049056 |
| field | 1048096 |
| packed | 1048096 |
| combined | 1049248 |

These desktop files use the same compiler settings. Their sizes are not
Android shipping APK/AAR size deltas.

## Remaining layout priorities

After the user-requested hybrid route, implicit class-field ranges are the
next bounded layout candidate: QQ has 16657296 directory bytes and 6904396
payload-capacity bytes, with no noncontiguous field rows in this census.
That observation alone does not establish the contract for every legal DEX.
Class-method rows include 2606 noncontiguous rows and cannot share an
unconditional range assumption. The 42 name tables occupy about 41.896 MiB
of requested layout including their fixed objects; their keys borrow raw
string bytes. Changing them requires a concrete representation/access
hypothesis, not an assumption that another hash table is always smaller.
Code/string offsets, raw proto lookup and forward-invocation capacity remain
later candidates. This batch does not exhaust all memory opportunities.

## Reproduction and evidence

[Execution plan](MEMORY-LAYOUT-EXECUTION.md),
[structured results](evidence/memory-layout/v1/summary.json),
[validation](evidence/memory-layout/v1/validation-summary.json),
[census](evidence/memory-layout/v1/census-summary.json), and
[raw evidence archive](evidence/memory-layout/v1/raw-evidence.tar.gz).
The archive contains drivers, frozen plans, raw samples/logs, complete fixture
oracles, build manifests and fixed-source increments. APKs, the QQ corpus and
compiled libraries/JARs/AARs are omitted; obtain those inputs separately.
Identical binary oracles use in-archive hardlinks; all names/content are
preserved and every entry is checked on readback. The archive manifest and
per-file hashes record that verification.
