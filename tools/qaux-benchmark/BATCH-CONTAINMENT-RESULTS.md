# Batch containment results

Replacing the intersection vector with `std::includes` gives a reproducible
improvement in QQ's 149-group batch API: the repeated API falls by 4.45%
[-7.09, -1.85] in confirmation, following 4.31% in the first batch. The whole
QQ lifecycle and process peak do not show a stable improvement. High-overlap
native fixtures benefit more; the three-group legacy QQ stage has almost no
work at this particular decision point. Keep the option default OFF and do
not add these percentages to the completed string or field experiments.

## Measurement identity and boundaries

The production source is fixed at `a997da6`. The same-source nine-switch
control has batch containment OFF, native SHA256 `1406de12...`; the candidate
adds only `DEXKIT_EXPERIMENT_BATCH_STRING_INCLUDES`, SHA256 `f114d521...`.
All ordinary single-string switches are OFF. The rebuilt control differs
from the older `2347a204...` binary; no old-control samples enter this comparison.

Ten sweeps retain all 120 process samples: each comparison has six balanced
fresh-process pairs and an independently ordered confirmation batch. QQ uses
the frozen APK, original adapters, JAR/JDK, four threads and memory probe.
All timed builds disable internal metrics, diagnostics and both trace options.
Native lifecycle includes create, builder preparation, complete output and
destruction, and close. OS file caches are not flushed. These are macOS ARM64
measurements; four-ABI Android assembly is build coverage, not runtime timing.
Intervals are exploratory paired-median bootstrap 95% intervals. Complete
records are retained under `evidence/next-round/batch-containment`.

## Frozen QQ results

| Boundary | First batch | Confirmation |
| --- | ---: | ---: |
| One round, create through close | -0.08% [-2.06, +7.54] | -1.13% [-2.67, +0.39] |
| Eleven rounds, create through close | -0.42% [-1.95, +1.51] | +0.19% [-2.80, +4.28] |
| Complete repeated APIs, rounds 2-11 | -1.17% [-1.66, -0.01] | +0.01% [-2.93, +4.68] |
| 149-group repeated API | -4.31% [-6.27, -3.79] | -4.45% [-7.09, -1.85] |
| Three-group repeated API | +1.34% [-0.08, +2.57] | +2.87% [-0.69, +20.01] |

The confirmation 149-group repeated API has process medians of about 125.55
ms for control and 118.91 ms for includes. The percentage above is the median
of paired changes, not a ratio of those separate medians. The first 149-group
API in the eleven-round run improves by 3.99% [-7.62, -2.95] in confirmation;
its first-batch interval crosses zero. The one-round confirmation also improves
that API, while the whole one-round lifecycle remains unresolved.

The legacy three-group first API has a positive confirmation estimate of
5.94% [+1.08, +50.87] in the eleven-round run, versus +0.83% [-1.08, +1.98]
initially. Its one-round confirmation estimate is -0.54% [-3.98, +5.05]. These
results do not establish a repeatable legacy-stage benefit or regression;
retain the positive estimates and uncertainty rather than hiding them under
the 149-group improvement. Neither QQ process-peak comparison has a repeated
improvement or regression.

## Controlled overlapping groups

The fixture contains three DEXes, 1,500 methods per DEX, ten methods per class
and four references to a seven-byte string per method. Each positive batch has
64 groups sharing seven single-character keywords: four groups match and sixty
require an additional missing keyword. Each iteration then submits a batch
whose keywords are absent. The miss fixture uses a different reference string
and never reaches the containment loop in either leg.

Confirmation over sixteen iterations:

| Workload | First API leg | Fully absent API leg | Create through close |
| --- | ---: | ---: | ---: |
| High-overlap methods | -18.02% [-20.01, -16.90] | -2.47% [-4.06, +1.54] | -17.96% [-19.96, -16.85] |
| High-overlap classes | -5.24% [-5.51, -4.31] | -2.16% [-6.17, +6.19] | -5.17% [-5.52, -4.30] |
| No-hit methods | -0.32% [-2.96, +0.71] | +0.18% [-2.13, +3.42] | -0.57% [-1.96, +2.18] |

The initial high-overlap lifecycle gains are 19.26% and 3.75%; both benefits
repeat. These concentrated workloads do not predict general application gains.
The no-hit first leg is not a positive result, despite the harness field name
`positive_ns`. Both no-hit legs stop at the existing empty-search-set guard;
their changes are not evidence of includes early termination. Native process
peak changes do not consistently resolve; the initial small no-hit peak
increase does not repeat.

## Actual work eliminated

Separate diagnostics report these counts per QQ query, identical in both
verification passes and across control/candidate:

| Work | 149-group batch | Three-group batch |
| --- | ---: | ---: |
| Method candidates after filtering/code checks | 1,921,832 | 1,921,832 |
| Candidates reaching the group loop | 14,644 | 1 |
| Group containment checks | 2,181,956 | 3 |
| Successful group matches | 1,631 | 1 |
| Control nonempty intersection vectors | 14,870 | 1 |
| Control intersection items | 15,103 | 1 |
| Sum of final vector capacity bytes | 241,792 | 16 |

Includes preserves the first four counts and materializes no intersection
vectors. It can also terminate the boolean comparison earlier, but comparison
counts were not instrumented: the observed API benefit is not assigned solely
to allocation savings. The three-group stage's large overall cost lies almost
entirely outside the changed loop.

Two native high-overlap method iterations produce 576,000 group checks and
36,000 matches; all 576,000 control intersections are nonempty, with 4,032,000
items and 73,728,000 summed final capacity bytes. Class counts are one tenth
of those values. Miss inputs have zero group checks. These are sums of final
temporary capacities, excluding growth history; they are not allocator-call
counts or simultaneous live/peak memory. Zero candidate counters for vectors
mean no vectors are materialized, not that mathematical intersections are
empty. Group counters exclude the all-empty direct fallback path.

## Correctness and build coverage

Before timing, control and includes pass 16 method/16 class batches against
independent raw fixture rows, parsed DEX code presence and complete ordered
serialized control bytes on small and wide pools. Cold, full-cache, repeated
and concurrent sequences agree. Standalone includes without the nine switches
and ASan/UBSan also pass the wide checks. The original ordered output hash is
`5301a430...` for every configuration.

After the source review, two cross-group Equal/Contains collision cases in
opposite orders extend the checks to 18 method/18 class batches. All 36 pass
on small/wide control and includes, and on wide standalone and ASan/UBSan
checkers relinked against the same immutable core archives. Their complete
ordered output hash is `6411d6a6...`; extracting the original 32 frames from
all six outputs reproduces `5301a430...` byte for byte. No measured native
library or workload executable changes for this test-only addition.

The candidate passes 71 JVM tests with zero failures/errors/skips, native/JAR
builds and release AAR assembly for arm64-v8a, armeabi-v7a, x86 and x86_64.
The Gradle desktop native hash matches the immutable measured artifact. Both
measured binaries pass eleven frozen QQ verification rounds with unchanged
ordered results and query flow. The initial checker build error and subsequent
source-review scope are recorded in BATCH-CONTAINMENT-REVIEW.
