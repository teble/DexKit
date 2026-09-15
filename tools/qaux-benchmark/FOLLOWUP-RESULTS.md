# Incremental optimization results

Status: work in progress. Only the first of five candidates has initial
measurements. All experiment switches remain OFF by default.

## 1. Field identity / reverse graph split: promising but conditional

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

Evidence is in `evidence/followup/field-v1/`. Component Gradle/Android checks,
sanitizers and final independent performance confirmation are still pending.

## Remaining candidates

Dense descriptor pointers, contiguous forward invocation rows, raw source-file
metadata and single-requirement invoke/caller matching have not been implemented
or measured in this phase. No final combination decision has been made.
