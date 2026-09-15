# Incremental optimization results

Status: work in progress. The first two candidates have scoped
decisions; three remain to be validated. All switches remain OFF by default.

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
consumer workload uses five repetitions. Independent confirmation is pending.

| Workload | Complete lifecycle change | Repeated API change | Peak footprint change |
| --- | ---: | ---: | ---: |
| QQ, 1 pass | -13.20% [-16.67, -10.94] | n/a | -7.40% [-8.07, -7.05] |
| QQ, 11 passes | -2.47% [-4.06, -1.12] | -0.01% [-1.29, +3.09] | -7.19% [-7.65, -6.77] |
| Synthetic forward only | -7.10% [-19.98, +18.29] | -0.23% [-26.38, +41.98] | -22.92% [-23.58, -22.01] |
| Synthetic forward then reverse | +4.95% [+2.51, +6.72] | +5.53% [+2.54, +8.35] | +0.47% [+0.07, +0.73] |
| Synthetic full-cache first | +1.80% [+0.17, +7.19] | +2.32% [+0.50, +10.23] | +0.18% [-0.86, +1.83] |

This is not yet a general no-time-regression choice. The consumer cases must
be investigated/confirmed instead of being hidden by QQ's unused-graph win.
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

## Remaining candidates

Contiguous invocation rows, raw source metadata and one-requirement relation
matching still need independent decisions. The final selected combination
will be measured directly against best5.
