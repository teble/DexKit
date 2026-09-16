# Ordinary find extraction: results

The two shortcuts are independent of the candidate pipeline at
`47eb54bf6e2a5d1d5db160b4f7589bb910b6d4d2`. They recover useful local benefits:
proven-empty queries avoid a queued task, and moving nonempty Bean vectors
avoids copying their payloads. The measured benefit is strongest for class
results and methods with parameters. QQ does not show a repeatable overall
speedup. QueryRun and its exception recovery machinery have been removed.

## Decision and configuration

Recommend adding both shortcuts to the existing Small configuration when these
query patterns matter. Keep the candidate pipeline and optional slicing as
separate opt-in experiments. The four measurement switches retain OFF defaults
on this benchmark branch; enabling either shortcut does not enable the pipeline.

| Variant | SKIP_EMPTY_CANDIDATES | MOVE_FIND_RESULTS | CANDIDATE_PIPELINE / CANDIDATE_SLICES |
| --- | --- | --- | --- |
| control | OFF | OFF | OFF / OFF |
| empty | ON | OFF | OFF / OFF |
| move | OFF | ON | OFF / OFF |
| both | ON | ON | OFF / OFF |

CMake uses the `DEXKIT_EXPERIMENT_` prefix. The Small profile already enables
INVERTED_STRINGS and INVERTED_STRING_RANGES; Empty bypass requires
INVERTED_STRINGS. The move switch has no candidate-framework dependency.
To add both shortcuts to an existing Gradle Small invocation, append:

```text
-PexperimentSkipEmptyCandidates=ON -PexperimentMoveFindResults=ON
-PexperimentCandidatePipeline=OFF -PexperimentCandidateSlices=OFF
```

The Small control library is byte-identical to the previous `7d536bf` Small
library (SHA-256 `fc6f407b5691d9d0e5e5462229b81ed6ca1a9eadcf55b3bb2f058b60d7b300c1`).
All four timed variants use the same nine accepted flags plus inverse strings
and guarded ranges. They use Apple clang 21, GNU C++20, Release `-O3 -DNDEBUG`,
arm64 and a macOS 11.0 deployment target. Both metrics switches and diagnostics
are OFF. The harness checks compiler settings, executable deployment target and
input hashes before timing.

## Independent measurements

There are 40 sweeps / 480 fresh processes: 36 preplanned sweeps, followed by
four explicit supplementary sweeps for nonempty method parameter vectors.
Each sweep uses six balanced pairs; the independent confirmation repeats the
same workload with a different order. No measured samples were excluded.
Native cases run 16 positive and 16 negative calls; API times below are sums
of 16 calls. Lifecycle also includes bridge creation, setup, serialization,
result consumption and close. QQ runs the full 149-string / 10-chain corpus
with one or eleven passes and four threads. Compilation and other validation
are finished before the timing phase. File caches are not flushed.

Changes are medians of paired percentages, with exploratory 95% bootstrap
intervals in brackets. They need not equal the ratio of the two marginal
medians. Results establish behavior on these finite workloads, not a universal
speedup across DEX files or hardware.

| Isolated shortcut and workload | Main | Confirmation |
| --- | --- | --- |
| Empty: pure negative method API, warm | -83.34% [-86.80, -78.96] | -86.18% [-89.19, -83.26] |
| Empty: guarded H<=1 method negative API | -77.84% [-79.94, -74.58] | -76.76% [-80.00, -73.21] |
| Empty: guarded H<=1 class negative API | -75.58% [-81.37, -69.50] | -77.05% [-79.68, -73.48] |
| Move: 1,499 class results, lifecycle | -5.66% [-29.70, -3.36] | -8.26% [-9.61, -5.53] |
| Move: 4,497 zero-argument methods, warm lifecycle | +0.81% [-0.04, +1.53] | +0.85% [-0.88, +2.51] |
| Move: 4,497 four-parameter methods, warm lifecycle | -3.58% [-4.29, -0.76] | -4.90% [-5.77, -3.68] |

In the pure Empty confirmation, the negative API sum falls from 0.1285 to
0.0185 ms, about 8.03 to 1.16 microseconds per call. Its full lifecycle falls
from 5.922 to 5.509 ms in that batch, but the first batch has a wide interval
crossing zero. Guarded positive queries do not establish a gain: the class
positive API medians change +13.53% and +9.78%, with both intervals crossing
zero. A faster negative leg does not imply that its positive partner got faster.

The class confirmation lifecycle medians are 7.529 to 6.968 ms. With four
parameters per method they are 43.071 to 40.818 ms. Those are meaningful payload
copies to remove: ClassBean owns method/field/interface vectors, and MethodBean
owns its parameter vector. The zero-argument case removes no parameter-buffer
allocation and shows no speedup; its positive API changes are +0.89% and
+0.93%, so a small cost remains possible.

The parameter supplement was added after observing that all original method
stress cases had empty parameter lists. It uses the same frozen engines and
workload executable, with four parameters (`I`, `J`, `java.lang.String`, `int[]`).
Both batches show a benefit. The new DEX checksum passes dexdump, existing
zero-parameter fixture bytes remain identical, and complete result bytes plus
parameter types are checked against input DEX metadata before timing.

| Both shortcuts, lifecycle | Main | Confirmation |
| --- | --- | --- |
| 1,499 classes | -6.43% [-19.29, +53.59] | -7.34% [-8.43, -4.80] |
| 4,497 four-parameter methods | -3.28% [-6.59, -2.09] | -3.47% [-4.68, -2.93] |
| Guarded H<=1 method | -2.76% [-4.49, +1.33] | -3.50% [-5.94, +0.33] |
| Uneven 16-DEX method workload | -0.10% [-0.43, +0.24] | -0.09% [-1.14, +1.04] |

The first combined class sweep includes substantial timing variation; its
wide interval is retained. Its confirmation improves, consistent with the
independently repeated move-only class benefit. The uneven workload has no
established gain or cost. The two shortcuts should not be credited with the
previous candidate pipeline's scheduling behavior.

## QQ and memory

| QQ lifecycle versus Small | Main | Confirmation |
| --- | --- | --- |
| empty, 1 pass(es) | +0.29% [-2.10, +1.97] | +0.48% [-1.89, +3.25] |
| empty, 11 pass(es) | +0.36% [-2.19, +5.35] | -0.24% [-1.23, +2.28] |
| move, 1 pass(es) | +2.00% [+1.52, +5.67] | -0.03% [-1.72, +2.90] |
| move, 11 pass(es) | -0.15% [-1.42, +2.39] | -2.18% [-3.44, +3.16] |
| both, 1 pass(es) | +1.08% [-1.48, +3.99] | +0.62% [-2.00, +3.48] |
| both, 11 pass(es) | +1.35% [-0.83, +4.50] | +0.48% [-1.11, +3.45] |

The move-only one-pass main sweep regresses by 2.00%, but this does not repeat
in confirmation. There is no repeatable QQ-wide improvement. Both shortcuts'
confirmation medians are 1407.1 to 1403.5 ms for one pass, and 4218.0 to
4222.0 ms for eleven passes; their paired intervals both cross zero.

The diagnostic QQ run proves that Empty bypass removes 40 otherwise queued
tasks per pass (440 over eleven passes) from the guarded ordinary string path.
It changes a small part of the total workload; Batch submission is unchanged.

Both-shortcut confirmation process peak footprint is 1535.96 to 1535.76 MiB
for one pass, and 1568.92 to 1562.95 MiB for eleven passes. These paired changes
are -0.13% [-0.39, +0.12] and +0.03% [-0.64, +0.46], respectively. Native
move-only peak footprint changes for the class and parameter cases also have
intervals crossing zero. No repeatable total-memory increase or saving is
established. These are process measurements including JVM, mapped DEX and
caches, not just temporary candidate storage or newly allocated payloads.

## Implementation and normal completion

- Empty bypass sets the task count to zero only after the existing planner
  returns proven Empty. Admission, scopes, findFirst and fallback semantics
  remain unchanged. The direct exact-class path and Batch are unchanged.
- Ordinary method/class future results use move iterators during concatenation.
  Output ordering, vector growth and descriptor representative selection stay
  the same. Field results and exact-class direct result concatenation are unchanged.
- QueryRun's 189-line header is deleted, including exception recording, catches,
  rethrows, capture counters and the additional condition-variable drain.
  The candidate coordinator consumes its normal futures and detaches the
  executor while QueryContext is alive.
- Candidate tasks use a small packaged-task wrapper. Its optional callable is
  moved onto the worker stack, so owning captures are destroyed before the
  future publishes the result. Retaining a completed wrapper/shared future
  cannot retain candidate reservations. Legacy callbacks hold borrowed inputs
  and finish their scoped query work inside the task body. No worker waits for
  child work. Candidate execution is noexcept and adds no recovery policy.

The generic existing SubmitQueryTask also instantiates its skip-result branch
and requires a default-constructible result. CandidateOutcome deliberately has
no default empty state, so the ordinary-only wrapper uses the same packaged-task
submission mechanism directly. It introduces no early-exit path or extra promise.

The simplified full candidate pipeline is correctness-tested in this follow-up.
Its performance is not retimed here; the measurements above use pipeline OFF.
The prior Pro review covers the earlier implementation and is kept as a
[historical record](CANDIDATE-PIPELINE-REVIEW.md), not a review of this change.

## Validation and completion

- [x] Extract both shortcuts and remove the exception machinery.
- [x] Eight native configurations, including the separate pipeline and
  ASan/UBSan slicing configurations, pass their ordered-result and runtime
  checks. Three focused runtime checks compile with `-fno-exceptions`.
- [x] Independent string (90 cases) and Batch (36 cases) oracles agree on
  small/wide fixtures. Admission checks agree for 58 cases in three index
  states on single-/dual-DEX fixtures, including scopes and frozen decisions.
- [x] All 63 tested proven-Empty submissions change from one task to zero,
  including 15 class cases, while preserving results. Real consumer checks
  preserve original slices, complete root truth and ClassDef ordering.
- [x] Retained-wrapper/shared-future cleanup, move-only budget release, and
  normal prepare/validation completion pass with one/four workers and an
  externally owned shared scheduler. Subsequent queries still work.
- [x] Small and candidate-pipeline Gradle configurations each pass 71 JVM tests
  with no failures or skips. Both compile all four Android ABIs; the pipeline
  configuration also passes `:dexkit-android:assembleRelease` and AAR inspection.
- [x] Six QQ configurations pass eleven-loop ordered result verification.
  The parameter supplement checks 40,482 method records / 4,500 distinct
  methods in each of four configurations, including ASan/UBSan; full result
  bytes match. Its timed count and checksum also match the independent manifest.
- [x] All 40 sweeps / 480 processes complete. No measured sample is excluded.

A preliminary untimed parameter checker incorrectly required 4,497 distinct
methods across every query. The 90-query suite includes match-all queries and
correctly returns all 4,500 methods; 4,497 applies only to the timed positive
string query. The corrected check and its original diagnostic are both archived.
The archive also retains the pre-freeze helper compilation diagnostic, fixed
before the validated engine was committed.

[Evidence manifest](evidence/ordinary-find-small/v1/manifest.json) records the
raw archive and integrity hashes. It contains all accepted samples, validation
records, build manifests, scripts, source identity and synthetic fixtures; it
does not include the QQ APK or compiled libraries/JARs/AARs.
