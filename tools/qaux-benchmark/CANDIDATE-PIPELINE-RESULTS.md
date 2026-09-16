# Ordinary candidate execution: results

The bounded FindMethod/FindClass refactor is implemented at
`82d6afb49d9faa5e49ee899ae948675d490ac99a`. Candidate storage, complete root-string
truth, task lifetime and ordered consumption now have separate contracts.
Successful preparation keeps validation in the same worker by default; optional
re-slicing uses the original ranges. Batch and the B + warm H<=1 admission rule
are unchanged. Multi-string seed selection and predicate fusion remain outside
this change.

There is no established QQ speedup. Optional re-slicing has no repeatable latency
benefit in the evaluated multi-candidate workloads and raises peak footprint.
Keep it disabled. The combined path has a modest class-output improvement and a
small uneven-DEX cost; it is an execution foundation, not a generally faster
planner. Both new experiment switches remain default OFF.

## Configuration and execution

All timed engines include the existing nine-option combination plus
INVERTED_STRINGS and INVERTED_STRING_RANGES. They differ only in the new options:

| Variant | Candidate pipeline | Successful re-slicing |
| --- | --- | --- |
| Small control | OFF | OFF |
| Combined | ON | OFF |
| Slices | ON | ON |

CMake names are `DEXKIT_EXPERIMENT_CANDIDATE_PIPELINE` and
`DEXKIT_EXPERIMENT_CANDIDATE_SLICES`; Gradle properties are
`experimentCandidatePipeline` and `experimentCandidateSlices`. Pipeline requires
INVERTED_STRINGS; slicing requires pipeline. Enabling the pipeline leaves exact
declaring-class, explicit-scope and findFirst entrances on their existing paths.

Selection is frozen before execution. All-Legacy queries retain their original
submission path. An admitted source either finishes in its preparation worker,
publishes immutable candidates for original-range consumers, or restores the
original ranges on a capability/budget rejection. Proven Empty needs no worker
task. One-worker execution, known H<=1 and one occupied range stay inline.
Workers never wait for child tasks or candidate credit. The caller uses a bounded
FIFO window of at most twice the worker count and merges in original DEX/slice
order. Method descriptor representative selection remains unchanged.

The full root-string bitmap is independent of slicing and residual filters.
Methods enumerate Method IDs; classes enumerate sorted ClassDef ordinals while
retaining complete Type-ID truth. Each consumer binds its own query/DEX/entity/
matcher-vector proof. Candidate lifetime and budget ownership do not borrow
QueryContext. QueryRun drains accepted input captures before detaching; completed
futures and retained outer worker closures do not retain those inputs.

## Measurement and input correction

There are **52 accepted sweeps and 624 fresh-process samples**: six balanced
randomized pairs per sweep, 36 main sweeps and 16 independently seeded
confirmation sweeps. The confirmation set was specified before measurement; it
covers the six substantive native cases and QQ one/eleven-pass workloads in each
comparison. The remaining small/cold/one-worker guards have main observations
only. Native workloads use sixteen positive/absent pairs. Warm setup includes an
explicit inverse-index warmup query and its destruction. API legs consume and
destroy results; lifecycle also includes query destruction and bridge close.

The platform is macOS ARM64, Apple clang 21 Release `-O3 -DNDEBUG`, a common
macOS 11.0 deployment target, and JBR 17.0.6 for QQ. QQ replays all 149 string
groups and ten query chains, with four workers. Native cases select one or four
workers. Diagnostics/internal metrics are off in all timed artifacts. Builds,
tests and archive creation do not overlap accepted timing runs. OS file caches
are not flushed. These are host measurements; Android was built and packaged,
not timed on a device.

A post-run input audit found that a newly linked adapter for the old Small archive
had the host's default macOS 26.0 deployment target, while CMake workloads used
11.0. All **22 affected native sweeps / 264 samples were preserved and excluded**,
then rerun with the matching CMake control workload. That control's engine library
is byte-identical to frozen old Small. QQ used the unchanged dynamic libraries,
and combined-to-slices used matching CMake executables; those samples were
retained. The workload driver now checks executable deployment targets and common
compiler settings before timing. Discarded adapter results do not support any
performance claim in this report.

Negative changes mean faster/smaller. Percentages are medians of paired relative
changes with exploratory paired bootstrap 95% intervals. Absolute figures are
marginal medians, so their ratios need not equal the paired percentages. An
interval crossing zero is unresolved; the earlier A/A calibration also showed
small time drift. First API, repeats, setup, positive/absent calls, create/close,
RSS and physical footprint remain available in the full summaries. Preparation,
matching and output are not separately timed inside native API calls.

## QQ and existing guards

| Comparison / metric | Main paired change and interval | Confirmation paired change and interval | Confirmation medians |
| --- | ---: | ---: | ---: |
| Small -> combined, QQ 1 pass | +0.48% [-0.14, +2.15] | -1.19% [-6.50, +2.26] | 1423.836 -> 1417.380 ms |
| Small -> combined, QQ 11 passes | -0.40% [-7.14, +2.19] | +0.31% [-1.05, +5.89] | 4373.928 -> 4340.730 ms |
| Combined -> slices, QQ 1 pass | -0.06% [-0.90, +0.74] | -1.59% [-11.27, +4.16] | 1516.930 -> 1452.667 ms |
| Combined -> slices, QQ 11 passes | +0.32% [-4.35, +1.36] | +1.58% [+0.13, +2.00] | 4156.387 -> 4221.983 ms |

The combined refactor has no established QQ lifecycle benefit. Re-slicing's
11-pass confirmation is +1.58%, while its main interval crosses zero; this is
neither evidence for enabling it nor a universal zero-overhead result.

The diagnostic QQ replay contains only eleven candidate-pipeline queries: one
per pass, each with a single already-indexed posting in one DEX and Empty in the
other forty. All eleven complete inline, with **zero separately queued validation
tasks**. Other ordinary queries retain Legacy, and Batch keeps its existing flow.
This corpus therefore checks integration and overhead, not parallel candidate
consumption. The eleven-pass Batch/chain stage summaries are retained separately.

| Comparison / metric | Main paired change and interval | Confirmation paired change and interval | Confirmation medians |
| --- | ---: | ---: | ---: |
| Small -> combined, onInitView warm | +1.26% [-6.98, +2.53] | -0.49% [-5.32, +3.64] | 10.766 -> 10.801 ms |
| Combined -> slices, onInitView warm | +0.53% [-0.04, +3.34] | +1.26% [-1.93, +5.78] | 10.013 -> 10.254 ms |

The old 4500 nested sample still selects Legacy. Small -> combined lifecycle is
-0.81% main and +2.37% confirmation, both unresolved; its positive leg is -1.93%
and +0.45%, also unresolved. The absent leg is +0.62% main (unresolved) and +5.75%
[+3.31, +13.34] confirmation. Keep that observation visible; it does not show a
new fallback or successful-candidate slicing benefit. No universal no-regression
claim is made.

## Combined execution and optional re-slicing

Small -> combined, four workers. The cross-slice fixture returns 4497 methods or
1499 classes. The uneven fixture contains one 10000-method DEX followed by fifteen
128-method DEX files.

| Comparison / metric | Main paired change and interval | Confirmation paired change and interval | Confirmation medians |
| --- | ---: | ---: | ---: |
| 4497 method results, warm | +0.22% [-0.90, +0.65] | +0.80% [-0.34, +5.48] | 34.264 -> 34.386 ms |
| 1499 class results | -6.21% [-8.27, -3.98] | -4.86% [-7.65, -1.56] | 7.446 -> 7.171 ms |
| 16 uneven DEX files | +1.43% [-0.48, +3.18] | +2.21% [+1.43, +3.64] | 437.596 -> 447.671 ms |
| Old 4500 nested guard | -0.81% [-4.27, +6.07] | +2.37% [-0.54, +3.66] | 1796.416 -> 1838.452 ms |

The class-output improvement repeats. It includes changed Bean movement and
consumption costs, so it must not be attributed to parallelism: this variant
validates inside its preparation worker. Uneven DEX lifecycle rises +1.43% main
(interval crossing zero) and +2.21% confirmation. The confirmation positive and
absent API totals rise +2.01% and +3.21%. Bounded FIFO preparation and waiting for
the current source's validation before refilling are plausible contributors;
these measurements do not isolate their individual costs.

Combined -> slices, four workers:

| Comparison / metric | Main paired change and interval | Confirmation paired change and interval | Confirmation medians |
| --- | ---: | ---: | ---: |
| 4497 method results, warm | +0.30% [-0.73, +1.60] | -0.84% [-2.16, +0.65] | 34.502 -> 34.364 ms |
| 1499 class results | +2.62% [-2.45, +6.47] | +1.22% [-1.81, +5.42] | 7.157 -> 7.323 ms |
| 16 uneven DEX files | -0.52% [-4.79, +8.81] | +0.13% [-0.47, +2.87] | 445.110 -> 446.182 ms |

The diagnostics confirm five method consumers or three class consumers for the
cross-slice positive query. The uneven positive query has sixteen preparations,
fifteen inline completions and ten queued consumers for the large DEX. Thus the
physical split was actually exercised, despite the lack of a repeatable time
benefit. One-worker runs and the H<=1 cases queue no second-stage work.

Forced capability/budget rejection is validated for fallback ranges, output order
and cleanup. It is **not timed as a production speedup**. In particular, the old
nested guard cannot stand in for a preparation-fallback benchmark.

## Single candidates and empty results

These API figures total sixteen calls after explicit warmup:

| Comparison / metric | Main paired change and interval | Confirmation paired change and interval | Confirmation medians |
| --- | ---: | ---: | ---: |
| Method H=1, positive APIs | -0.47% [-12.22, +23.44] | +10.49% [-9.54, +43.48] | 0.152 -> 0.162 ms |
| Method H=1, absent APIs | -75.50% [-78.15, -69.43] | -70.95% [-77.42, -64.00] | 0.119 -> 0.033 ms |
| One large class, positive APIs | +16.21% [-12.12, +33.70] | -12.49% [-26.26, +11.00] | 0.232 -> 0.209 ms |
| One large class, absent APIs | -70.54% [-75.34, -68.68] | -76.76% [-83.05, -69.28] | 0.124 -> 0.031 ms |

The matched reruns leave positive H=1 timing unresolved; they do not establish a
stable hit-path slowdown or speedup. Absence is consistently faster because proven
Empty no longer queues a worker. That improvement must not be relabeled as a
positive single-candidate gain. Method H=1 lifecycle is -1.36% main (unresolved)
and -3.10% confirmation; warmup and initialization dominate this short workload.
The standalone Empty guard improves warm lifecycle -6.05% in the main set; it was
not independently repeated. Tiny/cold and one-worker observations remain in the
full matrix without a general performance claim.

## Memory and budget

Peak process physical footprint:

| Comparison / metric | Main paired change and interval | Confirmation paired change and interval | Confirmation medians |
| --- | ---: | ---: | ---: |
| Re-sliced methods | +11.14% [+9.86, +15.07] | +17.81% [+10.24, +23.74] | 6.063 -> 7.181 MiB |
| Re-sliced classes | +6.22% [+4.55, +7.50] | +5.77% [+2.57, +9.01] | 5.142 -> 5.438 MiB |
| Small -> combined, QQ 11 | -0.27% [-0.75, +0.63] | -0.08% [-0.50, +0.64] | 1565.536 -> 1564.802 MiB |
| Combined -> slices, QQ 11 | +0.01% [-0.86, +0.40] | +0.08% [-0.62, +0.46] | 1564.435 -> 1565.200 MiB |

Re-slicing's method footprint increases +11.14% main and +17.81% confirmation;
class footprint increases +6.22% and +5.77%. Results/futures and allocator behavior
contribute to these process peaks. They are not a measurement of candidate arrays
alone, nor a leak finding. QQ peak changes are unresolved. No general memory
reduction is established.

The 64 MiB per-query reservation accounts for explicitly listed arrays: bitmap
payloads, keyword-plane headers, last-string IDs, cold-index counts/seen IDs, the
single output-group entry, class-ID conversion, slice records and Prepared storage.
It covers retained preparation results and consumers until the last owner releases
them. It excludes persistent indexes, matcher/trie caches, temporary associative
containers and trie-hit buffers, result metadata and allocator overhead. It is not
a total candidate-work or process limit, and concurrent queries have independent
budgets. QQ peaks at 8856 reserved candidate bytes; its persistent string indexes
still retain 5609448 bytes over 41 DEX files. All recorded runs release reservations
to zero.

## Validation and review

- Seven final native configurations, plus the frozen Small reference, agree with
  independent raw-row oracles and complete ordered bytes: 90 ordinary and 36 Batch
  cases on both small/wide fixtures, and 58 admission cases in each of three states
  on single/multi-DEX fixtures. The states are fresh, full cache with inverse still
  cold, and explicit inverse warmup.
- All seven pass index checks over 51 layouts / 5100 intervals, capture/future
  cleanup, controlled Submit failures, independent budgets and original ranges.
  No-exceptions preparation/consumption checks pass. Preparation/validation failure
  gates also pass with the real scheduler held outside QueryRun, so executor
  destruction cannot hide a missing drain by joining its pool.
- Four diagnostic configurations pass eight public cross-slice queries and 32
  injected production-consumer runs each, including exact slice counts/boundaries,
  reversed ClassDefs, whole-domain recursive truth and query identity. Nested
  production selection is separately asserted Legacy. The reordered fixture tests
  the parser's enumeration behavior; it is not an Android-runnable APK.
- Existing cross-DEX/root-vector, publication/frozen-decision, bitmap-budget,
  reversed-class-order and Batch checks pass. ASan/UBSan and the standalone
  pipeline/range configuration pass. Four eleven-pass QQ verification runs match
  frozen results and control flow with no API errors.
- `:dexkit:cmakeBuild :dexkit:jar :dexkit:test :dexkit-android:assembleRelease`
  pass: 71 JVM tests without skips/failures/errors, plus arm64-v8a, armeabi-v7a,
  x86 and x86_64. All five CMake caches have the expected thirteen options enabled.
  The initial `9227210` Android attempt failed because of direct try/throw in the
  no-exceptions build. The final implementation preserves Android's existing
  compiler policy and passes all four ABIs.
- [The Pro review](CANDIDATE-PIPELINE-REVIEW.md) read the fixed `0dc49ff -> 9227210`
  implementation and found no confirmed new ordinary semantic/lifetime blocker.
  Its budget-accounting and real-executor test gaps were addressed locally in
  `82d6afb`, together with the Android fix. Pro did not review those follow-up
  changes or execute these validations/measurements.

The inherited scheduler-internal allocation-failure boundary remains: QueryRun
cannot repair dispatch bookkeeping after arbitrary internal OOM. Its accepted-work
cleanup guarantee applies to the new admitted/mixed path, not to unchanged
all-Legacy submission code. Complete error recovery for the underlying scheduler
was not added.

## Evidence and reproduction

- [All accepted metrics and QQ stages](evidence/candidate-pipeline/v1/summary.json)
- [Validation](evidence/candidate-pipeline/v1/validation-summary.json),
  [source identities](evidence/candidate-pipeline/v1/source-identity.json), and
  [diagnostic paths](evidence/candidate-pipeline/v1/diagnostics-summary.json)
- [Archive manifest](evidence/candidate-pipeline/v1/manifest.json),
  [execution contract](CANDIDATE-PIPELINE-EXECUTION.md), and
  [original Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)

The archive preserves accepted and excluded observations, exact commands/hashes,
compiler settings, the Android failure and correction, synthetic fixtures and
reproduction sources. It excludes the proprietary QQ APK and compiled native,
JVM and AAR binaries. For current-source reproduction, build the three variants
above with `DEXKIT_BENCHMARK_STRING_WORKLOAD=ON`, generate the candidate fixtures,
and use the archived main/confirmation plans with `workload_sweep.py`. Diagnostic
and sanitizer builds remain separate. The user's main checkout is unchanged.
