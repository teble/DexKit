# Incremental optimization results

Status: complete. All five candidates were validated independently against
best5, followed by a direct combination measurement and independent confirmation.
All experiment switches remain OFF by default.

The preferred additional experiment is **SINGLE_RELATION**. Its confirmation
reduces QQ lifecycle by 5.87% for one pass and 18.33% for eleven passes. A
separate **QQ-conditional combination** adds field identity splitting and
contiguous invocation rows: confirmation reduces lifecycle by 22.50% and
24.11%, with peak footprint down about 10.1%. It retains a reverse-field
consumer cost and mixed-row memory increases, so it is not the general choice.

| Candidate | Decision | Evidence that determines the scope |
| --- | --- | --- |
| Field identity/reverse split | Conditional | QQ wins; reverse consumers still cost about 1.5--2% independently. |
| Dense descriptor pointers | Exclude from general preference | Full SSO output lifecycle regresses about 12.5% for two repetitions. |
| Contiguous invocation rows | Conditional | QQ wins; mixed/giant matcher costs and mixed-row memory increases reproduce. |
| Raw source-file views | Exclude from general preference | Wide source matching warm cost reproduces at about 3.1--4.3%. |
| Single-requirement relation matching | Preferred addition for evaluated workloads | QQ and single-requirement gains reproduce; control regressions do not reproduce. |

Performance measurements are from this macOS arm64 host and the frozen QQ
9.3.55/QAuxiliary workload. Four Android ABIs were built; Android device
performance was not measured. Lifecycle includes create, initialization,
queries, result release and close, under the existing benchmark protocol.
Intervals are exploratory paired bootstraps, not universal guarantees.
See [reproduction](FOLLOWUP-REPRODUCE.md) and the detailed decisions below.

## Field decision after the bounded follow-up

Retain the field split as an optional prototype for workloads that do not
consume reverse field rows. Do not include it in a generally preferred
combination that is described as having no observed time regression.

`bd0f8ae` restores the original nonempty binding filter when local reverse
rows are already published. Identity-only initialization still retains all
successful bindings for later use. This avoids needless full-first work, but
does not establish that temporary bindings caused the warm output regression.
`f01e59f` adds diagnostic-only warm-up counts and splits each workload API leg
into first and repeated timings. The timed native code remains exactly the
same as the binding revision: SHA256
`1ab574a057b427e451c80ed96d0de3fff1124433703f3f2f69a9b61c5923ea73`.

The revised candidate passed all six relation sequences and both independent
full descriptor oracles, including ASan/UBSan runs. Warm forward/reverse
repetitions explicitly assert zero additional local initialization, identity
resolution and aggregation jobs. The native/JAR build, a freshly executed
71-test JVM suite and release AAR builds for all four ABIs passed with the
field split enabled. After the final JAR build, best5 and the revised native
library each passed eleven complete QQ verification passes again.

Six fresh balanced pairs at independent seed 2026091524 measured the revised
binary. The relation fixture now uses 64 repetitions, with no other builds or
heavy tests running during measurement:

| Revised candidate vs best5 | Complete lifecycle | Repeated APIs | Peak footprint |
| --- | ---: | ---: | ---: |
| QQ, 1 pass | -9.19% [-12.78, -7.34] | n/a | -7.78% [-8.06, -6.87] |
| QQ, 11 passes | -5.38% [-10.17, -2.78] | -1.42% [-7.70, -0.65] | -7.56% [-7.95, -7.00] |
| Synthetic forward only | -2.56% [-12.60, +14.48] | -0.88% [-11.53, +26.84] | -22.63% [-23.17, -21.77] |
| Synthetic forward then reverse | +1.95% [+1.66, +2.31] | +1.96% [+1.52, +2.34] | +1.80% [-4.02, +8.89] |
| Synthetic full-cache first | +1.54% [+1.00, +3.36] | +1.54% [+1.07, +3.68] | -1.40% [-5.48, +3.28] |

Repeated reverse getters alone regress +1.99% [+1.72, +2.41] in the late case
and +1.54% [+1.10, +3.73] in the full-first case. This is not hidden deferred
construction: diagnostic executions establish that the warm phases do not
perform cache jobs. The exact low-level cause remains unproved. An independent
64-repeat run of the first version also reproduced about 2% regression; direct
first-version/revised-version comparisons did not establish its removal.
No further field-specific layout tuning is part of this phase.

Detailed samples, isolated binding-filter comparisons, validation hashes,
component log and build manifests are in `evidence/followup/field-final/`.
The earlier first-version data below remains source-pinned historical evidence.

## First field prototype: initial measurements

The new best5 control has exactly the previous preferred native SHA256
`331f61535e1821da655eae1d39bc0edd691a4b09270175cb13da86277e34e22f`.
Both non-diagnostic libraries passed the complete frozen QQ corpus for eleven
passes with four query workers. Diagnostic consumption checks found zero
reverse-row matcher/getter reads in that workload.

The split replaces 122,450,104 bytes of logical reader/writer capacity with
2,125,824 bytes of retained ordered field bindings in this run. These logical
capacities are not a process peak estimate. The source is `064c0d8`; native
field-split SHA256 is
`8e612b2f577ac224372dda285c45a8054f2cbfc2afa603e6c851f392c3f7d14b`.

Initial measurements use six balanced fresh-process pairs, seed 2026091521.
Percentages compare the field split with best5, and include 95% exploratory
paired-median bootstrap intervals. Positive values are regressions. All valid
samples are retained. QQ uses one or eleven complete passes; the synthetic
consumer workload uses five repetitions. This historical batch was followed by
the revised independent confirmation reported above.

| Workload | Complete lifecycle change | Repeated API change | Peak footprint change |
| --- | ---: | ---: | ---: |
| QQ, 1 pass | -13.20% [-16.67, -10.94] | n/a | -7.40% [-8.07, -7.05] |
| QQ, 11 passes | -2.47% [-4.06, -1.12] | -0.01% [-1.29, +3.09] | -7.19% [-7.65, -6.77] |
| Synthetic forward only | -7.10% [-19.98, +18.29] | -0.23% [-26.38, +41.98] | -22.92% [-23.58, -22.01] |
| Synthetic forward then reverse | +4.95% [+2.51, +6.72] | +5.53% [+2.54, +8.35] | +0.47% [+0.07, +0.73] |
| Synthetic full-cache first | +1.80% [+0.17, +7.19] | +2.32% [+0.50, +10.23] | +0.18% [-0.86, +1.83] |

This first batch was not a general no-time-regression choice. Its consumer
cases prompted the bounded investigation and revised confirmation above.
In the field workload reports, `negative_ns` is the reverse getter leg, not a
negative query. Full-cache setup, delayed construction, all result destruction
and close are charged to complete lifecycle.

The independent best5 oracle and candidate match every byte of the forward,
reverse and caller/invoke results for both a small and an adverse three-DEX
fixture. This covers all raw reference and definition IDs, duplicate edges,
unused references, duplicate definitions, differing local IDs, and the old
unresolved-reference cursor behavior. Six sequences cover forward-first,
reverse-first, late full-cache, racing requests, reverse warm-up queued behind
an active forward admission, and forward entry after reverse readiness.
Repeated reverse/full access does not append extra edges.

Evidence is in `evidence/followup/field-v1/`. The subsequent revision's checks
and performance confirmation are reported above, not assigned to this binary.

## Dense descriptor pointer decision

Reject the pointer prototype from the generally preferred combination. Keep
its switch OFF. It has a clear sparse-coverage memory benefit, but introduces
repeatable high-coverage lifecycle regressions. The QQ time effect is not
established as non-increasing either; this is not an unconditional improvement.

Source `5560403` uses dense atomic pointers, stable independent standard string
objects, the original shared 32 cold locks and acquire/release publication.
It copies the original constructed string as the dense optional path does;
it does not introduce arenas, pages, raw lookup or a different character
capacity policy. Normal artifact SHA256 is
`f39deafcea582d0d835e2ae6e8ea8fac8c6d4d713930ac58bfe4b4c38c313212`.
The new best5 control remains byte-identical to the original `331f615...`.
The field split is OFF in this comparison.

Both normal libraries passed all eleven frozen QQ verification passes.
The candidate passed the two complete frozen symbol byte oracles, a new
full byte oracle for 120,000 short descriptors generated by the original
all-flags-off control, both six-sequence relation oracles, and all these
checks under ASan/UBSan. Eight overlapping readers verify content, stable
same-slot publication, retained addresses and exactly one construction per
slot. Diagnostics confirm all 120,000 high-coverage entries actually use SSO
on this host; descriptor length alone is not the evidence for that claim.

Fresh-process balanced pairs use seeds 2026091525 and 2026091526, six pairs per
workload in each batch. No heavy builds or tests ran during timing. Values
below are median paired changes with exploratory 95% bootstrap intervals;
positive time changes are regressions. Full samples of both batches and the
other long-output/hot/prefix workloads are retained in `evidence/followup/pointer/`.

| Independent confirmation vs best5 | Complete lifecycle | Repeated APIs | Peak footprint |
| --- | ---: | ---: | ---: |
| QQ, 1 pass | -1.34% [-5.40, +1.90] | n/a | -6.27% [-6.71, -6.05] |
| QQ, 11 passes | +1.58% [-1.19, +3.54] | +2.05% [+0.04, +4.66] | -5.59% [-6.43, -5.33] |
| Full SSO output, 2 repetitions | +12.49% [+11.59, +13.02] | -0.04% [-2.06, +2.20] | +2.09% [+1.95, +2.30] |
| Full SSO output, 16 repetitions | +2.01% [+1.65, +2.23] | -0.54% [-0.73, -0.24] | +1.92% [+1.20, +2.05] |
| Wide cached-text lookup, 256 repetitions | +1.33% [+0.75, +8.21] | +1.72% [-1.27, +10.00] | +0.69% [+0.34, +0.95] |

The first batch independently measured +11.87% [+8.66, +14.56] and
+2.11% [+1.81, +2.62] lifecycle for the two SSO workloads. In confirmation,
first output is about 15.6--16.9% slower; the 120,000 separate object deletions
raise close from roughly 0.15 ms to 1.6 ms. Repeated output is stable or slightly
faster, so the complete-workload cost cannot be hidden by reporting warm hits
alone. A general claim about the cause of every warm lookup/QQ variation would
still be unsupported.

For QQ, only 1,628 of 4,092,900 entries are materialized. The pointer index is
32,743,200 B (31.23 MiB), with 235,441 B for string objects and external
character capacity, plus 83,968 B for shared locks. The corresponding dense
slot and publication difference is about 97.5 MiB of logical capacity; process
peak savings are the separately measured 5.6--6.3%, not that logical total.
At full SSO coverage, 8-byte pointers plus 24-byte string objects nearly equal
the old 32-byte optional slots plus 1-byte ready flags, while adding 120,000
object allocations. Allocator metadata and rounding are outside these logical
counts. The output workload retains the method result while producing the
field result; its peak includes both buffers equally in both variants. Cache
census after results are released and process peak are separate evidence.

The Pro source review found no new correctness blocker and agreed that the
high-coverage cost should constrain adoption instead of triggering another
arena rewrite. Empty cache/empty string and deterministic allocation-failure
checks in `61b7b92` passed under ASan/UBSan. PTR alone, with STRUCTURAL and
FAST_HITS OFF, passed the dense and both symbol concurrency checks. Native/JAR,
71 freshly executed JVM tests and four release Android ABIs passed with best5
plus PTR enabled. The rebuilt JAR SHA256 remains `aad51ff2...`, exactly the one
used by all frozen verifications and paired samples.

## Contiguous invocation row decision

Retain this as a conditional prototype, not a general no-time-regression
improvement. QQ benefits reproduce, but mixed and giant-row matcher costs
also reproduce. The mixed fixture additionally exposes increased memory.
No further invocation allocation policy is introduced to rescue this result.

Source `a89257e` directly appends invocation IDs into checked contiguous rows,
preserves duplicate positions and order, and borrows spans in the invoke
solver. Caller storage and its solver remain unchanged. This changes both
persistent row representation and the invoke solver's target copying, so
matcher effects cannot be assigned entirely to storage layout. The normal
artifact is `a9b869422851199657afa0e676cb14fe9b610e93533dad1d413c5eeaffe7a99b`;
the independently rebuilt best5 is still byte-identical `331f615...`.
Both the field split and descriptor pointers are OFF.

All frozen QQ outputs passed eleven verification passes. The candidate and
ASan/UBSan builds match independent best5 full-byte oracles for all raw
reference/definition IDs' ordered invoke/caller results on tiny, mixed and
131,072-entry-row fixtures. Nested queries, counts, Equal, absent/empty
requirements and duplicate positions also pass. `aec239e` extends this to
24 query cases and five initialization sequences, including an actual
invokes-only internal admission followed by queued caller construction.
Every retained row's address, length and content remains unchanged after
caller and full-cache warm-up. Two identical requirements for a single
available position fail, while the same single requirement succeeds.
All old query oracle bytes remain unchanged. Native/JAR, 71 freshly executed
JVM tests and all four release Android ABIs pass with this switch enabled.
The rebuilt JAR remains byte-identical to the measured one.

Both complete batches use six balanced fresh-process pairs, independent seeds
2026091527 and 2026091528, and no heavy concurrent builds. The confirmation
below reports paired median changes with exploratory 95% bootstrap intervals;
all 28 workload summaries and every valid sample are retained in
`evidence/followup/invocations/`.

| Independent confirmation vs best5 | Complete lifecycle | Repeated APIs | Peak footprint |
| --- | ---: | ---: | ---: |
| QQ, 1 pass | -7.39% [-11.93, -4.80] | n/a | -2.63% [-2.90, -2.15] |
| QQ, 11 passes | -4.96% [-8.27, -1.50] | -2.65% [-6.99, -0.51] | -2.88% [-3.29, -2.51] |
| Mixed invoke output, 4 repetitions | +6.21% [+0.34, +12.46] | -0.36% [-2.95, +2.79] | +21.15% [+9.79, +22.22] |
| Mixed invoke matching, 32 repetitions | +1.03% [+0.59, +1.51] | +1.02% [+0.54, +1.53] | +13.33% [+4.97, +19.63] |
| Mixed caller matching, 32 repetitions | +2.05% [+1.87, +3.06] | +2.08% [+1.90, +3.09] | +22.44% [+1.28, +25.68] |
| Giant invoke matching, 32 repetitions | +0.55% [+0.36, +0.86] | +0.52% [+0.34, +0.85] | -8.35% [-11.95, -5.56] |
| Giant caller matching, 32 repetitions | +1.78% [-2.27, +2.20] | +1.78% [-2.38, +2.21] | +1.37% [-15.75, +11.69] |

The first batch independently measured mixed caller lifecycle +1.63%
[+1.22, +2.30] and giant invoke lifecycle +0.88% [+0.61, +1.96]. These
regressions are not inferred from intervals crossing zero. Some tiny and
getter intervals remain unresolved; mixed output lifecycle did not reproduce
as a clear regression in both batches, whereas its memory increase did.
The low-level cause of the warm matcher differences remains unproved.

QQ's invocation index falls from 62,400,744 B to 31,200,372 B, but payload
capacity rises from 58,875,724 B to 67,239,936 B. Net logical capacity falls
by 22,836,160 B (21.78 MiB), not by the entire old payload. Live buffers fall
from 1,651,594 to 123. Allocator effects and complete process peaks are
separately measured. In the mixed fixture, old rows have 393,224 B of payload
capacity versus 524,288 B globally; the small index saving cannot offset that
capacity increase. Growth overlap is per index, not a concurrent process peak,
and these counts alone do not explain the full measured footprint difference.

Pro read the actual implementation and found no new correctness blocker.
Its deferred-caller and positional-conflict suggestions are implemented and
pass in both representations and under ASan/UBSan. Its ordered-getter oracle
request was already covered by the completed relation checker on all three
new fixtures. Review agreement is not used as performance evidence.

## Raw source-file metadata decision

Do not include this in the generally preferred combination. The removed
source-view table reduces memory, but source matching has a reproducible warm
cost and QQ time is not established as non-increasing. Keep the default-OFF
prototype as a measured tradeoff; do not claim that a raw indexed read is
universally cheaper than a view lookup.

Source `7437dfa` removes only the source-file view side table. The class Bean
reuses its already loaded ClassDef, while the source matcher reads through
type_def_idx, ClassDefs and strings after the existing undefined-type guard.
Missing indexes, defined empty strings, original raw byte encoding, view
lifetime and local/cross-DEX class selection are preserved. The normal native
artifact is `3b0b1b356117caf95f1f633640cca645f95fbc4950a2bffa26aeae120d3a1c84`.
Best5 is still byte-identical `331f615...`; field split, descriptor pointers
and contiguous invokes are all OFF in this independent comparison.

Independent best5 full-byte comparisons pass on a small three-DEX fixture and
a version with 60,000 additional classes. They cover 16 source queries, all
raw type IDs, canonical class lookup, three initialization/concurrency orders,
missing/empty/UTF/MUTF-8/long strings, undefined references and duplicate
definitions. Known source bytes have independent expected values. ASan/UBSan
passes both oracles and both old frozen symbol oracles. Both normal libraries
pass all eleven frozen QQ passes. Native/JAR, 71 freshly executed JVM tests
and all four Android release ABIs pass. The JAR is unchanged.

Six balanced fresh-process pairs per case use independent seeds 2026091529
and 2026091530. All valid samples, sixteen stage summaries and manifests are
retained in `evidence/followup/sources/`. Confirmation increases hot getter
repetitions from 4,096 to 65,536 to expose its warm cost; other repetition
counts are unchanged. Intervals below are exploratory 95% paired bootstraps.

| Confirmation vs best5 | Complete lifecycle | Repeated APIs | Peak footprint |
| --- | ---: | ---: | ---: |
| QQ, 1 pass | -0.09% [-2.32, +0.47] | n/a | -0.87% [-1.22, -0.18] |
| QQ, 11 passes | +1.37% [-0.81, +3.00] | +1.07% [-0.78, +2.85] | -0.26% [-0.96, -0.17] |
| Wide source matching, 64 repetitions | +3.19% [+0.56, +4.80] | +4.34% [+1.14, +5.79] | -4.30% [-4.40, -4.09] |
| Wide class output, 8 repetitions | -0.20% [-2.10, +1.05] | -0.02% [-2.85, +1.17] | -0.76% [-3.32, +1.49] |
| Wide hot getter, 65,536 repetitions | -0.66% [-1.82, +0.27] | +0.63% [+0.10, +0.73] | -4.36% [-4.36, -4.29] |
| Small source matching, 4,096 repetitions | +0.56% [-2.11, +3.24] | +0.46% [-2.15, +3.19] | +0.00% [-1.98, +2.02] |

The first batch independently measured wide source matching warm cost
+3.15% [+0.07, +4.90], with complete lifecycle +1.94% [-1.17, +4.13].
The independent warm regression and confirmation lifecycle regression support
the scoped decision. Hot getter and class-output effects are not generalized
from one narrow interval; their full results remain available. Source-hot
work is in first/repeated timings, not the unused positive/negative fields.

The removed QQ table is 694,054 views at 16 bytes: 11,104,864 B (10.59 MiB)
of logical array capacity. Raw string bodies are unchanged. This is distinct
from the separately measured process peak. The source matcher's additional
dependent reads are a code fact; they are not a complete causal explanation
of every timing difference.

Pro's actual source review found no new correctness blocker. Its suggested
fixture guard is added, and an independent raw DEX parse confirms Absent uses
0xffffffff while Empty uses valid index 0 in both fixtures. All timed native
workload manifests were checked to have diagnostics and both metrics options
OFF; workload_sweep now enforces both metrics gates itself. The new checks
do not change any already frozen measurement binary.

## Single-requirement relation matching decision

Retain SINGLE_RELATION as the preferred additional experiment for the evaluated
workloads. It has independently confirmed QQ and single-requirement gains,
with no reproduced regression in the exercised getter/multiple-requirement
controls. Small intervals that include regressions remain uncertainty, not
proof of universal non-increase. The switch remains OFF by default.

Production source `c323a10` takes the original ordered judge directly when
there is exactly one requirement, after the original count/size checks.
It checks Equal only after a witness is found. Null, empty and multiple
requirements keep the original paths. This avoids requirements preparation,
right-side copying and generic solver work arrays. The old one-row solver
already stopped at its first witness; this does not automatically reduce
predicate calls. The normal library SHA256 is
`7e132b063d7c1178bd43361f8583bdb299ffc2b22ae52b8d14474198065ef131`.
The final diagnostic-only additions in `645ca4f` rebuild to exactly that same
normal library. All other follow-up switches are OFF in this comparison.

Two independent batches, seeds 2026091531 and 2026091532, each use six balanced
fresh-process pairs and the same repetition counts. Both QQ modes and all
24 native cases were repeated: 52 complete stage summaries and every valid
sample are in `evidence/followup/single/`. Early-hit workloads run only the
same positive query in both variants; late/miss workloads use a different,
nested predicate. Only each mode's own A/B comparison measures the change.
These include full scans, output and teardown; they are not pure solver
microbenchmarks.

| Confirmation vs best5 | Complete lifecycle | Repeated APIs | Peak footprint |
| --- | ---: | ---: | ---: |
| QQ, 1 pass | -5.87% [-9.72, -4.83] | n/a | -0.13% [-0.51, +0.15] |
| QQ, 11 passes | -18.33% [-19.66, -17.49] | -23.25% [-25.10, -22.00] | -0.40% [-1.66, +0.07] |
| Tiny invoke late/miss, 1,024 repetitions | -20.83% [-22.32, -16.11] | -21.04% [-22.47, -16.30] | -7.21% [-8.26, -2.27] |
| Tiny caller late/miss, 1,024 repetitions | -9.12% [-11.66, -7.19] | -9.25% [-11.76, -7.40] | -7.01% [-8.33, -5.16] |
| Mixed invoke early, 1,024 repetitions | -44.98% [-47.98, -43.67] | -45.56% [-48.66, -44.28] | -1.32% [-5.21, -0.67] |
| Mixed caller early, 1,024 repetitions | -66.24% [-70.34, -64.60] | -68.04% [-71.98, -66.38] | -21.26% [-24.37, -11.18] |
| Giant invoke early, 1,024 repetitions | -77.35% [-77.76, -76.18] | -80.29% [-80.68, -79.15] | -17.72% [-18.00, -14.72] |
| Giant caller early, 1,024 repetitions | -85.81% [-86.30, -84.65] | -87.89% [-88.55, -87.52] | -1.28% [-12.73, +0.00] |

The first QQ batch independently measured lifecycle -6.00% and -20.12%.
Confirmation late/miss lifecycle gains on mixed and giant rows are smaller,
about 1.8--2.9%. Multiple-requirement controls remain unresolved or slightly
faster. The first giant invoke getter's +5.61% [+0.05, +12.51] lifecycle did
not reproduce: confirmation is +0.45% [-0.86, +2.78]. Getter controls do not
support a getter speedup claim. QQ peak memory is likewise unresolved; the
preferred addition is principally a time improvement.

Both normal libraries pass all eleven frozen QQ passes. The initial 24-case
and complete ordered-getter oracles match frozen best5 results on all three
fixtures, including ASan/UBSan. Both old symbol oracles and concurrency checks
pass. Native/JAR, 71 freshly executed JVM tests and all four release Android
ABIs pass with SINGLE_RELATION enabled.

`645ca4f` adds count-allowed and count-rejected single requirements, checks the
exact judge order for late/miss/allowed/rejected rows, and adds a single caller
whose nested field predicate requires reverse-field initialization. All
29 cases pass against the independent original-solver control, the single
prototype and the conditional combination, including both sanitizer builds.
The previous 24 case buffers are unchanged. Rejected counts record zero judge
calls; the allowed early case records one. The field-split combination starts
with forward field identity only and verifies actual RW readiness after the
nested caller query. Held invocation spans remain unchanged after late
caller/full initialization.

Untimed case counts clarify the multiple-workload labels: tiny forward rows
have only one aEarly position and fail the duplicate-aEarly requirements;
tiny caller rows can have several run positions and succeed (two returned
methods in the fixture). Mixed/giant forward rows supply successful repeated
positions. Variant 10 is the separate single-position conflict. No timed
workload binary was altered to add these diagnostic records.

## Direct conditional combination

The conditional configuration is best5 plus FIELD_IDENTITY_SPLIT,
COMPACT_INVOKES and SINGLE_RELATION. POINTER_DESCRIPTORS and RAW_SOURCE_FILES
remain OFF. Its normal artifact, built from `645ca4f`, is
`2347a204a16ecf3b0855189ee2c3e62b27e8c1cdff476146ecb7964e6d7422ba`.
It is compared directly with the unchanged best5 `331f615...`; no individual
percentages are added. The single-requirement and combination figures come
from their respective paired experiments, not an indirect incremental estimate.

Two independent six-pair batches, seeds 2026091533 and 2026091534, cover QQ
and nineteen native consumer cases. All 42 stage summaries and samples are
in `evidence/followup/combinations/`. Confirmation results follow.

| Combination vs best5 | Complete lifecycle | Repeated APIs | Peak footprint |
| --- | ---: | ---: | ---: |
| QQ, 1 pass | -22.50% [-24.62, -11.19] | n/a | -10.12% [-10.46, -9.76] |
| QQ, 11 passes | -24.11% [-25.63, -22.72] | -23.72% [-24.36, -22.84] | -10.18% [-10.52, -9.83] |
| Field forward only, 64 repetitions | -3.57% [-7.45, -1.61] | -1.64% [-6.79, +1.41] | -22.34% [-22.89, -22.11] |
| Field forward then reverse, 64 repetitions | +2.35% [+0.70, +5.29] | +2.25% [+0.42, +5.12] | +3.19% [+0.53, +8.30] |
| Field full-cache first, 64 repetitions | -0.18% [-2.71, +1.52] | +0.07% [-2.88, +1.18] | -5.18% [-7.29, -2.49] |
| Mixed invoke output, 4 repetitions | +2.76% [-3.44, +6.99] | +0.52% [-0.31, +30.81] | +21.81% [+14.72, +23.88] |
| Mixed invoke multiple, 32 repetitions | -1.87% [-4.00, -0.12] | -1.92% [-3.56, -0.37] | +9.50% [+1.80, +16.08] |
| Giant invoke late/miss, 32 repetitions | -0.15% [-1.15, +0.63] | -0.10% [-1.25, +0.73] | -14.27% [-17.02, -11.94] |

The first batch independently measured QQ lifecycle -24.37%/-24.03% and
peak footprint -9.70%/-10.00%. The forward-then-reverse cost also reproduced:
first lifecycle +2.26% [+1.97, +2.93]. Mixed invoke output memory increased
in both batches (first +17.48% [+5.18, +21.81]). These are real restrictions
on the combination's scope. First-batch full-first field and giant invoke
matching time regressions did not reproduce as clear regressions in the
second batch; their results are retained as uncertainty, not silently omitted.

The combination passes all eleven frozen QQ verification passes, the final
29-case/6-sequence invocation checker, all ordered getter oracles on three
invocation and two field fixtures, both source metadata oracles, and both
old symbol oracles. ASan/UBSan runs pass the same relevant checks. Joint
validation includes actual delayed caller publication, retained spans, count
rejection without predicate calls, and nested reverse-field initialization.
Native/JAR, 71 freshly executed JVM tests and all four release Android ABIs
pass with the combined flags. The JAR SHA256 remains
`aad51ff2f604056be1a1b2cbb0a41adae32ac17e855e7eac8a6db10898383e9a`.

## QQ after completing deferred field work

This follow-up answers whether the memory benefit survives when the delayed
reverse-field work is actually performed. The original QQ corpus has no such
consumer. The new opt-in `--final-field-rw` operation runs after all one or
eleven unchanged QQ passes and before close. It resolves
`Lcom/tencent/mobileqq/data/MessageRecord;->msg:Ljava/lang/String;` and fetches
both its readers and writers through public result APIs. `FieldGetMethods`
enters execution with `kRwFieldMethod | kMethodUsingField`, initializing and
aggregating the required reverse indexes across all 41 DEX files. Field
lookup, index construction, result return and close are included in complete
lifecycle. This is one final reverse operation, not repeated reverse-query
throughput; the separate repeated-consumer counterexamples still apply.

The 399 readers and 313 writers match an independent best5 ordered oracle.
All eleven original QQ passes also match the unchanged frozen results for
each normal variant. Separate diagnostic runs prove that both variants end
with identical reader/writer index and payload capacities: 122,450,104 bytes
(116.78 MiB). The old combination without the final operation had zero
reader/writer capacity. Thus its original roughly 130 MiB process saving from
field deferral cannot be assumed after construction. Logical capacity and
process peak remain distinct measurements.

No production code or native artifact changed. Harness source is `28e0c32`;
the immutable best5, field-only and combination binaries are respectively
`331f615...`, `1ab574a...` and `2347a204...`. All diagnostic and internal metric
switches are OFF in timed binaries. Their previously completed native/JVM/AAR
checks remain applicable; this follow-up reran the modified benchmark's full
QQ and field-result verification, rather than rebuilding the library.

Two independent batches use six balanced fresh-process pairs per cell,
seeds 2026091535 and 2026091536. All 96 valid process samples are retained.
No builds, heavy checks or archiving run alongside measurement. Negative
percentages favor the candidate; intervals are exploratory paired-median 95%
bootstrap intervals. Confirmation results:

| Configuration vs best5 | QQ passes, then one reverse operation | Complete lifecycle | Peak footprint | Paired peak saving |
| --- | ---: | ---: | ---: | ---: |
| Field split only | 1 | -0.35% [-1.89, +1.96] | +0.15% [-0.40, +0.47] | -2.62 MiB |
| Field split only | 11 | +0.24% [-4.15, +0.94] | -0.37% [-0.57, -0.18] | 6.44 MiB |
| Field split + compact invokes + single relation | 1 | -11.17% [-13.17, -8.80] | -2.97% [-3.12, -2.72] | 50.77 MiB |
| Field split + compact invokes + single relation | 11 | -18.87% [-20.39, -17.23] | -3.36% [-3.48, -2.94] | 57.64 MiB |

First batch:

| Configuration vs best5 | QQ passes, then one reverse operation | Complete lifecycle | Peak footprint | Paired peak saving |
| --- | ---: | ---: | ---: | ---: |
| Field split only | 1 | +2.22% [+0.56, +5.28] | -0.15% [-0.27, +0.46] | 2.60 MiB |
| Field split only | 11 | +1.42% [-0.15, +3.01] | -0.22% [-0.37, -0.13] | 3.85 MiB |
| Field split + compact invokes + single relation | 1 | -9.31% [-12.36, -4.70] | -2.81% [-3.23, -2.52] | 47.77 MiB |
| Field split + compact invokes + single relation | 11 | -20.22% [-22.21, -18.73] | -3.08% [-3.34, -2.78] | 52.86 MiB |

The field-only switch does not preserve its original 7.6--7.8% QQ peak
reduction once reverse indexes are built. Any remaining small footprint
difference must be weighed against the lifecycle figures above. The complete
combination's remaining gains belong to that directly measured combination;
they do not establish the isolated benefit of field deferral or the untested
combination with that switch removed.

For this QQ workload followed by one reverse operation, the complete
combination still improves both complete time and peak footprint in both
batches. Confirmation saves about 51/58 MiB while reducing lifecycle time
11.17%/18.87%. The field-only configuration has no resolved one-pass peak
benefit, and only a small repeated-workload peak benefit (first 3.85 MiB,
confirmation 6.44 MiB). Its first one-pass time regression does not reproduce
as a clear regression in confirmation; both confirmation lifecycle intervals
cross zero. This does not support paying a known repeated-consumer cost in
exchange for the original 130 MiB saving when reverse indexes will be needed.

All summaries, samples, native manifests, the ordered field oracle and
diagnostic capacity proof are in `evidence/followup/field-tail/`. Its separate
`process-records.tar.gz` includes all eight stages, verification records and
the default-path smoke run. Every archive member and the archive itself were
read back and checked against `archive.json`. The original five-candidate
archives below retain their original 167-stage scope.

## Evidence and completion

Each of the five individual configurations and the conditional combination
passed the required native/JAR/JVM/Android build checks. Sanitizer claims here
are ASan/UBSan with the recorded options; no ThreadSanitizer or Android-device
runtime performance claim is implied. All full byte expectations remain
unchanged except for explicitly additional test cases, whose original buffers
are checked against the earlier frozen oracles.

`evidence/followup/process-records/` contains six deterministic compressed
archives for all 167 timing stages: 3,490 original JSON/log files. Every
archive and every member was read back and verified against its SHA256 and
size in `archive.json`. Diagnostic logs, fixture manifests, component logs,
compiler/native manifests and independent byte oracles are retained in the
corresponding candidate directories. APKs and generated native/JVM binaries
are not committed.

Production defaults, public APIs and schema are unchanged. The experiment
branch contains the scoped prototypes and reproducible evidence; it does not
constitute upstream adoption or a release.
