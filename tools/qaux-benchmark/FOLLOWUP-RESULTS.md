# Incremental optimization results

Status: work in progress. The first of five candidates has a conditional
decision; four remain to be validated. All switches remain OFF by default.

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

## Remaining candidates

Dense descriptor pointers, contiguous forward invocation rows, raw source-file
metadata and single-requirement invoke/caller matching have not been implemented
or measured in this phase. No final combination decision has been made.
