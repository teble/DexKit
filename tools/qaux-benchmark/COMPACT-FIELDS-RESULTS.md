# Contiguous forward-field results

Status: complete. This is one independent COMPACT_FIELDS increment over the
nine-switch control. It improves the original frozen QQ lifecycle and peak in
both finite batches, but increases native long-row process peak substantially.
Keep the workload-specific tradeoff; it is not a general memory optimization.

## Scope and method

Fixed source: 4247372bede8bf9460076825ee44c838e94df16b. The control enables
LAZY_DIRECTORIES, COMPACT_STRINGS, NEGATIVE_STRINGS, STRUCTURAL_DESCRIPTORS,
DESCRIPTOR_FAST_HITS, RAW_INTERFACES, FIELD_IDENTITY_SPLIT, COMPACT_INVOKES and
SINGLE_RELATION. Only COMPACT_FIELDS is added to the candidate. SINGLE_USING_FIELD,
new single-string/Prefix and batch-includes options remain OFF. Defaults remain
OFF in production build files. This candidate combines contiguous field storage
with borrowing its immutable rows in the existing solver; the timings cannot
attribute the entire change to either mechanism alone.

Normal native SHA256:

* control: `1406de12c38e356b1771209f2006c73a189975e4f90c892dcf75f48c3b4fab16`
* compact: `7a116237a34950c644feb04ce671207f05fed0a303005a21e824b800b4fdfd1d`

There are 22 sweeps and 264 fresh-process samples: eleven workloads, six balanced
pairs each, followed by an independently ordered six-pair confirmation batch.
No build, test, diagnostic or archive task ran concurrently with formal timing.
The same macOS arm64 compiler, O3 configuration, JBR 17, four query threads,
frozen QQ 9.3.55 corpus/APK, adapter/JAR and memory probe are retained. File caches
are not flushed. All trace and metrics options are OFF in timed artifacts.

Percent changes below are medians of paired candidate/control changes; negative
favors compact. Brackets are exploratory paired bootstrap 95% intervals, not a
proof of no regression. They are not ratios of the displayed unpaired medians.
Lifecycle includes construction, preparation, API work/output destruction, close
and final local object destruction. Peak is the measured process peak footprint;
it is not the sum of logical capacities. These are host measurements; Android
validation covers packaging/builds, not on-device performance.

## Frozen QQ replay

| Workload | Lifecycle main | Lifecycle confirmation | Peak main | Peak confirmation |
|---|---:|---:|---:|---:|
| QQ, 1 pass | -7.92% [-16.14, -1.64] | -8.96% [-13.21, -4.54] | -2.65% [-3.01, -2.27] | -3.09% [-3.32, -2.72] |
| QQ, 11 passes | -2.83% [-6.48, -0.89] | -1.48% [-3.18, -0.41] | -3.14% [-3.65, -2.33] | -3.44% [-3.65, -3.18] |
| QQ, 11 passes + final RW | -1.24% [-1.67, +1.69] | -1.38% [-3.28, -1.04] | -2.85% [-3.21, -2.56] | -2.85% [-3.60, -2.43] |

Confirmation lifecycle medians are 1,534.18 -> 1,400.25 ms for one pass and
5,921.95 -> 5,826.74 ms for eleven passes. Corresponding peak medians are
1,529.65 -> 1,485.26 MiB and 1,537.99 -> 1,484.79 MiB. Both ordinary QQ batches
resolve lower lifecycle and process peak.

The first optional_has_info API, which includes forward-field preparation,
improves 27.80% [23.14, 29.07] in the eleven-pass confirmation (160.13 ->
115.41 ms). Its repeated API does not establish a gain: confirmation instead
shows +2.94% [+2.20, +5.02], while the main batch is +1.50% [-5.09, +2.69].
The ordinary replay's aggregate repeated API is -0.33% [-4.19, +1.57] in main
and +1.75% [-0.06, +2.58] in confirmation. Do not describe this as a warm-query
optimization merely because its cold preparation and lifecycle improve.

For QQ with a final RW operation, lifecycle points favor compact but the main
interval crosses zero. The final RW API alone is -0.16% [-12.13, +1.55] in main
and -4.13% [-8.43, -1.66] in confirmation; its incremental gain is not independently
resolved in both batches. Peak does improve in both batches. Retain the repeated
QQ API slowdown in this workload: +1.13% [+0.21, +3.71] in main and +1.20%
[+0.32, +1.34] in confirmation. Those APIs occur before the final RW leg; this is
not evidence that the later operation caused the earlier slowdown.

## Bounded native workloads

Long/short fixtures each have 3,627 defined methods; their bulk rows contain
256/2 uses respectively. Query workloads repeat 32 times. The getter-output
workload repeats eight times and returns all 921,631 ordered uses per pass;
method enumeration is included in setup. Sparse matching still scans the method
domain before selecting the small named subset. Relation workloads repeat
forward/reader/writer operations on the existing field-adverse fixture; full-first
includes InitFullCache in setup/lifecycle.

| Workload | Lifecycle main | Lifecycle confirmation | Peak main | Peak confirmation |
|---|---:|---:|---:|---:|
| Long, first witness | -15.19% [-22.92, -9.80] | -9.29% [-12.62, -7.49] | +55.88% [+54.81, +57.03] | +56.01% [+52.98, +57.01] |
| Long, two requirements | -0.77% [-2.31, +0.88] | -1.88% [-2.44, -0.53] | +53.76% [+52.54, +54.54] | +56.02% [+54.12, +56.47] |
| Long, sparse query | -15.84% [-21.20, -14.52] | -23.97% [-28.80, -15.50] | +62.30% [+58.33, +62.92] | +61.94% [+59.27, +62.27] |
| Short, first witness | -8.36% [-13.24, -3.21] | -9.83% [-16.37, -4.86] | -1.77% [-3.17, +0.26] | +0.95% [-6.51, +4.58] |
| Long, GetUsingFields output | -0.63% [-1.03, +0.19] | -0.53% [-0.91, -0.12] | +45.72% [+40.77, +47.49] | +44.56% [+43.68, +47.79] |
| Relation, forward only | -4.26% [-21.01, +4.50] | -12.12% [-13.60, -6.21] | +1.44% [-0.52, +2.24] | +1.44% [+0.78, +2.36] |
| Relation, forward then RW | -1.15% [-1.42, -0.73] | -0.90% [-1.28, -0.63] | -1.14% [-4.27, +3.83] | -2.00% [-10.46, -0.43] |
| Relation, full cache first | -0.44% [-0.85, +1.68] | +0.00% [-1.25, +0.80] | -1.14% [-6.50, +3.95] | +0.46% [-4.27, +7.13] |

Long first-witness and short-row lifecycle/API improvements repeat. The long
sparse lifecycle benefit also repeats, but its repeated API intervals cross
zero in both batches; the clearer benefit is initial preparation. Two-requirement
and complete getter lifecycle intervals cross zero in main, so their small
confirmation improvements do not establish the same general result. Getter
first API improves about 1% in both batches; repeated serialization dominates.

All four long-row peak regressions repeat. Confirmation medians include
13.70 -> 21.38 MiB for first-witness matching, 12.86 -> 20.81 MiB for sparse
matching, and 14.66 -> 21.16 MiB for getter output. These are material costs
relative to those bounded processes even though the absolute QQ footprint is
larger and moves in the other direction.

The forward-then-RW relation lifecycle has a small repeated improvement, but
its first RW leg does not resolve a change in either batch. Full-first lifecycle
remains unresolved. Forward-only time and its small peak increase resolve only
in confirmation; retain both intervals instead of declaring a universal gain.

## Separate storage diagnostics

All counts below come from untimed DIAGNOSTICS builds. Reverse-row census values
agree between control and compact. Entries count method rows; live buffers are
allocated backing buffers at observation time, not malloc calls.

| Field-forward storage | Control bytes | Compact bytes | Change |
|---|---:|---:|---:|
| QQ row columns | 62,400,744 | 31,200,372 | -31,200,372 |
| QQ payload capacity | 55,446,792 | 62,128,128 | +6,681,336 |
| QQ columns + payload | 117,847,536 | 93,328,500 | -24,519,036 |
| Long fixture columns + payload | 7,460,096 | 8,432,148 | +972,052 |
| Short fixture columns + payload | 144,896 | 109,076 | -35,820 |
| Relation fixture columns + payload | 527,560 | 525,940 | -1,620 |

QQ has 2,600,031 method rows, 1,540,649 nonempty rows and 5,475,675 uses. The
observed field-table live-buffer count changes from 1,540,690 to 123. Compact
records 755 payload growths and 62,127,800 inferred moved element-storage bytes;
its largest single-DEX old+new capacity overlap is 3,145,728 bytes.

The long fixture has 921,631 uses. Its compact pool records 42 growths,
8,388,600 inferred moved bytes and a largest single-DEX capacity overlap of
6,291,456 bytes. Its final payload capacity is larger than the old per-row total.
This is a relevant adverse mechanism, not a calculation of the process peak:
parallel DEXes, allocator behavior, columns and other objects also contribute.
The logical totals exclude container objects and allocator bookkeeping.

## Verification, review and evidence

All 48 independent method/class predicates and complete ordered GetUsingFields
rows pass for small, short, long and unresolved-field fixtures. The independent
validator directly decodes each fixture's instruction rows, then checks identity,
order, direction and multiplicity. Standalone and ASan/UBSan candidates pass the
small, long and unresolved cases. Both relation fixtures pass all six schedules
in diagnostic control/candidate and sanitized builds, including published-span
address/value stability across real queued RW, later Calls and InitFullCache.
The raw-ID relation coverage complements the getter fixture's unique definitions.

Both normal libraries pass eleven frozen QQ rounds with and without final RW;
399 ordered readers and 313 ordered writers agree with the independent control.
The required native build, JAR, 71 JVM tests and four-ABI Android AAR complete.
One disk-full event interrupted the initial Android build, long-check output
and QQ-tail record writing. After saving configs and removing obsolete generated
Android intermediates, the affected checks pass in separate v2 records. Failed
logs are retained; formal timing started only after recovery and all checks.

[COMPACT-FIELDS-REVIEW.md](COMPACT-FIELDS-REVIEW.md) records the fixed-source Pro
review and locally adopted diagnostic/coverage limits. The review did not rerun
tests or approve these performance figures. The same source remained fixed
through all measured artifacts and both batches.

[evidence/next-round/compact-fields/summary.json](evidence/next-round/compact-fields/summary.json)
contains every metric, paired sample change and interval, including the first and
repeated optional_has_info API. The adjacent archive manifest and raw evidence
preserve logs, fingerprints, finite commands, independent oracles, fixtures,
platform checks, disk recovery records and fixed source scripts. Proprietary QQ
APK and compiled binaries are omitted; their identities remain recorded.

Retain COMPACT_FIELDS as a workload-dependent experimental candidate: ordinary QQ
benefits, while dense long-row consumers pay a substantial peak cost. Do not add
its percentages to prior candidates or silently fold it into the next control.
The RW-only instruction-walk experiment uses the original nine-switch control
with COMPACT_FIELDS and SINGLE_USING_FIELD OFF.
