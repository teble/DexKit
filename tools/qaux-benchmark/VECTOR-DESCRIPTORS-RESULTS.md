# Independent vector descriptors: completed results

The hypothesis is partly supported. Sparse QQ retains its memory advantage and
wide/prefix descriptor lookup becomes faster than the current pointer hybrid.
The new backend still has material regressions against old dense, higher physical
peaks in several dense workloads, and an explicit native borrowing contract.
Keep this as a conditional experiment with VECTOR_DESCRIPTORS OFF by default;
this batch does not support replacing the default storage or promising that
high descriptor demand avoids substantial regression.

## Fixed comparison

All four normal artifacts use `4f7a82c44ab593c9db334e7623e06eede280220c` and identical Small14/compiler options.
`control` is the old dense descriptor backend within that same optimization
combination; it is not unmodified upstream master. `hybrid` is the preceding
direct-slot pointer hybrid, whose source behavior is preserved. `vector` uses
independent fixed strings/ready arrays after deferred reclamation.
`vector-no-promotion` keeps the new sparse allocator/accounting and borrowing
protocol while disabling conversion. Only their storage options differ.

The measured harness/review-check snapshot is `d0783697f539b7c7a255b64845416369cb94247b`.
Additive test and diagnostic-only refinements link the original frozen Core
archives. The added first-stage snapshot preprocesses out identically in all
four normal transition workloads; their original timed executables are retained.

Frozen plan SHA256: `8a67e4b3e34e06a6467956e3ba439dfd2e090d6f33e80e111a60976babd40ef4`.
All 82 sweeps / 984 fresh benchmark process samples completed, with zero excluded
successful samples. Six balanced AB/BA pairs per sweep were run in a main phase
and one independently seeded confirmation. All frozen inputs reconciled before
and after measurement. No builds, correctness runs, profiling or evidence
compression ran during the fixed timing batch. A separate four-process driver
preflight preceded the freeze and is identified as validation.

Negative changes favor vector. Percentages below are medians of paired changes,
not ratios of absolute medians. Values are main / confirmation. Bootstrap 95%
intervals are exploratory per-case estimates from six pairs without adjustment
for multiple comparisons. Repeated support means both intervals exclude zero
in the same direction; crossing zero is not proof of equivalence.

Lifecycle includes create, setup, API stages, result destruction and close.
Four calling threads perform four times the work; they are separate workloads.
QQ worker count is query-engine parallelism, not four simultaneous Java callers.
OS caches were not flushed. Measurements use Apple M1 / macOS 26.5.1, not an
Android device. Full intervals, absolute medians, MADs and all samples are in the
[summary](evidence/vector-descriptors/v1/summary.json) and raw evidence.

## Findings that determine the decision

- QQ peak footprint versus old dense is -9.06% to -8.78% across all six case/phase observations;
  every peak interval excludes zero. In p1/w4 this is about 130 MiB saved:
  1471.62 -> 1341.64 MiB in the main phase and
  1472.13 -> 1341.65 MiB in confirmation.
  QQ has zero descriptor conversions in diagnostics, so this supports retention
  of the sparse memory benefit, not acceleration from the dense vector.
- QQ p1/w4 lifecycle versus old dense changes -2.71% [-4.68, -1.01] / -1.50% [-3.15, -0.85].
  Other QQ lifecycle comparisons do not have both intervals excluding zero.
  Relative to the current pointer hybrid, QQ has no repeated lifecycle or peak
  difference excluding zero. Close improves versus old dense in p1/w1 and p1/w4;
  individual query stages are not generally faster.

- Wide lookup lifecycle versus pointer hybrid: -17.16% [-20.12, -12.63] / -16.31% [-19.39, -13.13].
  Repeated stages improve -36.58% [-38.27, -31.04] / -35.30% [-39.33, -31.94], but the first stage costs
  +102.45% [+89.79, +105.57] / +104.64% [+99.34, +112.11]. It contains a hit API then a miss API: the latter can
  convert and rebuild after the former filled the sparse generation.
  Direct old-dense residuals remain +32.50% [+26.16, +35.96] / +36.86% [+34.33, +40.19] for lifecycle and
  +15.98% [+5.77, +19.69] / +19.13% [+16.04, +23.72] for repeated stages. Vector reduces the residual; it does not remove it.
- Prefix lookup improves -8.74% [-9.69, -7.22] / -9.11% [-11.06, -8.13] versus hybrid, but remains
  +5.51% [+1.14, +8.64] / +5.28% [+3.16, +6.50] slower than old dense over the full lifecycle.
  Its repeated-stage residual versus old dense is not separately established
  in both phases. The first conversion/rebuild stage still matters.
- Narrow hot lookup changes -2.22% [-2.62, -2.01] / -2.07% [-2.52, -1.74] versus hybrid and
  +2.52% [+2.25, +3.21] / +3.19% [+2.76, +3.39] versus old dense. It does not convert; this small timing
  change cannot be credited to the dense backend.

- Long descriptor output is slower than hybrid: +8.89% [+7.76, +13.93] / +8.33% [+5.22, +16.67],
  and slower than old dense: +10.74% [+7.76, +12.79] / +9.35% [+7.02, +10.53]. The repeated stage also regresses.
- Full field output once then close costs +29.16% [+5.31, +67.97] / +39.52% [+4.05, +58.39] versus old dense.
  This one-caller/one-API case never converts: the regression already exists in
  sparse construction, API/Bean entry, output and close. It is not a rebuild cost.
  Two full field calls cost +50.69% [+31.12, +73.40] / +32.56% [+8.74, +49.50]; the second stage
  includes reclamation and fresh generation. Both are concrete counterexamples
  to a general high-demand no-regression claim.
- At 16 full method or field passes, lifecycle intervals cross zero versus both
  hybrid and old dense. Amortization is plausible, but these samples do not prove
  equal performance. Full method two-pass, changed working sets and four-caller
  results also retain broad intervals; do not turn their medians into guarantees.

- Against no-promotion, wide lifecycle improves -71.83% [-72.81, -70.30] / -71.64% [-72.28, -71.09].
  This shows why the dense path matters for repeated broad lookup. No other
  no-promotion lifecycle comparison has repeated interval support. Full-method
  two/sixteen-pass peak footprints improve about 5%, and close improves at
  sixteen passes, but they do not establish an end-to-end timing win.

## Physical memory is an additional cost

Logical reclamation succeeded: after conversion no old hash bucket, deque block,
directory, owner or character allocation remains owned by the cache. Physical
process peaks still increased. In particular:

| Case / comparator | Peak change, main / confirmation | Absolute peak MiB, baseline -> vector, main / confirmation |
| --- | --- | --- |
| hybrid-to-vector-wide | +81.73% [+81.47, +81.95] / +81.49% [+80.87, +81.73] | 9.19 -> 16.70 / 9.21 -> 16.71 |
| control-to-vector-wide | +83.48% [+83.11, +83.74] / +84.06% [+83.48, +84.30] | 9.09 -> 16.69 / 9.07 -> 16.70 |
| hybrid-to-vector-long-output | +30.33% [+30.19, +30.69] / +30.28% [+29.91, +30.41] | 34.83 -> 45.39 / 34.83 -> 45.36 |
| control-to-vector-field-full-2 | +5.56% [+5.53, +5.69] / +5.72% [+5.53, +5.83] | 29.24 -> 30.86 / 29.23 -> 30.88 |
| control-to-vector-method-full-2 | +2.93% [+2.85, +2.98] / +2.93% [+2.81, +3.08] | 32.00 -> 32.93 / 32.00 -> 32.95 |

The wide case is roughly +7.5 MiB despite smaller final descriptor structure storage; long output is
roughly +10.6 MiB versus hybrid. Eleven primary hybrid/vector peak comparisons
and ten old-dense/vector comparisons show increases in both phases. These
adverse samples are retained. This batch does not isolate allocator retention,
fragmentation and transient output allocations, so it does not assign all of
that physical increase to one cause. Freeing old logical contents does not
establish that the OS footprint immediately shrinks or peak memory improves.

Diagnostic wide/long-output runs discard 4096 descriptors with 7307264 requested
character bytes, then build 4096 dense values with the same final character
capacity. No old character owner is retained. The live character count is not
a bound on peak physical footprint during this history.

The byte threshold is a maintenance request, not a maximum memory budget. For
60000-member full output, a single API fills the whole sparse domain before
it can safely finish borrowing. Structural bytes at conversion reach 3800832,
although the new dense object/ready base is 1500000. SSO fields and methods
need no external character buffers. Waiting until the next safe admission
preserves old views but allows this overshoot and temporary allocation history.

## QQ results

| Comparison / workload | Lifecycle (95% interval), main / confirmation | Peak footprint, main / confirmation |
| --- | --- | --- |
| hybrid-to-vector-qq-p1-w4 | -0.62% [-5.49, +2.72] / +0.43% [-1.15, +3.49] | +0.03% [-0.84, +0.79] / +0.01% [-0.36, +0.18] |
| control-to-vector-qq-p1-w4 | -2.71% [-4.68, -1.01] / -1.50% [-3.15, -0.85] | -8.78% [-9.24, -8.59] / -8.88% [-9.10, -8.37] |
| vector-no-promotion-to-vector-qq-p1-w4 | +0.36% [-0.06, +2.10] / -0.68% [-1.86, +0.93] | -0.00% [-0.24, +0.29] / +0.12% [-0.39, +0.65] |
| hybrid-to-vector-qq-p11-w4 | -0.15% [-1.90, +0.58] / +0.17% [-2.27, +0.92] | +0.10% [-0.18, +0.37] / +0.03% [-0.47, +0.28] |
| control-to-vector-qq-p11-w4 | -0.30% [-1.47, +0.02] / -0.17% [-0.85, +1.06] | -9.06% [-9.50, -8.82] / -8.91% [-9.25, -8.24] |
| hybrid-to-vector-qq-p1-w1 | -0.03% [-1.65, +0.82] / -0.31% [-0.68, +0.57] | -0.04% [-0.65, +0.93] / -0.14% [-0.73, +0.15] |
| control-to-vector-qq-p1-w1 | -0.99% [-1.43, -0.10] / -1.06% [-1.58, +0.37] | -9.03% [-9.46, -8.84] / -9.03% [-9.13, -8.74] |

## Complete native results

First/repeated/close columns show paired median changes. All intervals and
absolute values remain in JSON. A dash means a zero/absent repeated stage.
`wide`, `prefix` and `hot` use hit/miss API pairs per pass. Transition full cases
use one member-domain API per pass. The changed case reads prefix 30000 IDs then
disjoint suffix 30000 IDs; low cases repeatedly read the same 64 scattered IDs.

### hybrid-to-vector

| Workload | Lifecycle (95% interval) | First stage | Repeated stage | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| wide | -17.16% [-20.12, -12.63] / -16.31% [-19.39, -13.13] | +102.45% / +104.64% | -36.58% / -35.30% | -14.48% / -12.31% | +81.73% / +81.49% |
| prefix | -8.74% [-9.69, -7.22] / -9.11% [-11.06, -8.13] | +94.33% / +93.64% | -13.45% / -13.84% | +629.83% / +686.93% | +74.72% / +74.56% |
| hot | -2.22% [-2.62, -2.01] / -2.07% [-2.52, -1.74] | -53.52% / -49.62% | -2.17% / -2.15% | -2.81% / -8.86% | -1.33% / -1.34% |
| long-output | +8.89% [+7.76, +13.93] / +8.33% [+5.22, +16.67] | +1.54% / -2.92% | +10.59% / +12.26% | -10.82% / -8.45% | +30.33% / +30.28% |
| sso-prefix | +0.54% [+0.01, +1.07] / +0.20% [-3.41, +2.73] | +3.24% / +2.98% | -0.21% / -0.76% | -15.63% / -9.53% | -0.39% / -0.19% |
| sso-scattered | -0.44% [-3.31, +5.54] / +0.08% [-1.77, +1.20] | -1.00% / +5.88% | +0.32% / -0.80% | +27.07% / +17.44% | -0.32% / -0.07% |
| narrow-w1 | +0.84% [-16.69, +4.41] / -1.82% [-14.02, +3.61] | -1.28% / -3.03% | +0.49% / -1.84% | -27.32% / -2.51% | -1.41% / -0.93% |
| narrow-w4 | +23.98% [-0.68, +46.57] / +18.23% [-8.72, +48.97] | -26.72% / +69.71% | +24.11% / +18.22% | -25.08% / -19.83% | -2.26% / -1.79% |
| method-full-1 | -13.13% [-24.04, +4.60] / -4.97% [-17.89, +3.67] | -9.49% / -4.18% | -- | -21.97% / -18.28% | +7.08% / +7.05% |
| method-full-2 | +19.39% [+8.02, +26.04] / +10.80% [-0.64, +36.70] | -1.27% / -0.25% | +75.81% / +35.52% | -18.51% / -31.72% | +8.29% / +8.26% |
| method-full-16 | +0.64% [-12.62, +8.29] / -1.05% [-7.18, +2.77] | -2.79% / -0.66% | +1.95% / -1.11% | -34.40% / -56.28% | +8.26% / +8.50% |
| field-full-1 | +3.49% [-25.82, +19.79] / +1.29% [-5.88, +16.98] | +3.66% / -1.65% | -- | -11.54% / +10.06% | +13.03% / +13.12% |
| field-full-2 | +15.71% [-0.33, +31.38] / +24.30% [+11.18, +66.06] | +1.52% / +19.06% | +51.64% / +76.54% | -32.50% / -24.35% | +13.02% / +13.13% |
| field-full-16 | +1.97% [-2.70, +17.72] / -0.48% [-21.69, +13.93] | +3.63% / +0.68% | +1.22% / -1.44% | +1.94% / -29.59% | +12.86% / +13.12% |
| method-changed-2 | +4.77% [-0.18, +27.45] / +2.08% [-13.75, +9.09] | -2.53% / +1.05% | +40.72% / -0.85% | -36.50% / -19.39% | +5.84% / +5.99% |
| low-id-w1 | +2.02% [-3.01, +5.60] / +0.26% [-3.28, +7.64] | +4.48% / -3.18% | +3.22% / -0.09% | +7.96% / +10.12% | +0.17% / +0.26% |
| low-id-w4 | -10.10% [-20.24, -4.76] / -6.79% [-22.69, +6.33] | -13.53% / +12.15% | -13.39% / -9.27% | +1.93% / -35.87% | +0.34% / +0.50% |
| method-full-w4 | +7.73% [-11.79, +23.10] / +27.27% [-2.55, +36.18] | +12.29% / +26.11% | +5.02% / +29.83% | -57.76% / -59.75% | +4.79% / +4.19% |

### control-to-vector

| Workload | Lifecycle (95% interval) | First stage | Repeated stage | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| wide | +32.50% [+26.16, +35.96] / +36.86% [+34.33, +40.19] | +107.46% / +107.92% | +15.98% / +19.13% | -3.61% / +3.61% | +83.48% / +84.06% |
| prefix | +5.51% [+1.14, +8.64] / +5.28% [+3.16, +6.50] | +107.56% / +98.23% | +0.26% / +0.18% | +648.14% / +768.08% | +76.11% / +76.29% |
| hot | +2.52% [+2.25, +3.21] / +3.19% [+2.76, +3.39] | +54.43% / +49.48% | +2.78% / +3.41% | -15.48% / +2.83% | -5.46% / -5.98% |
| long-output | +10.74% [+7.76, +12.79] / +9.35% [+7.02, +10.53] | +3.66% / -0.46% | +11.98% / +11.62% | -0.11% / +3.17% | +24.13% / +24.14% |
| method-full-1 | -6.14% [-18.19, +18.17] / -8.84% [-23.61, +22.98] | +4.15% / -8.06% | -- | -3.12% / +6.69% | +1.83% / +1.91% |
| method-full-2 | +11.37% [-1.03, +28.27] / +9.67% [-3.62, +27.92] | -0.86% / -4.87% | +48.56% / +59.69% | -24.48% / -53.18% | +2.93% / +2.93% |
| method-full-16 | +3.40% [-1.49, +16.02] / -1.19% [-8.44, +15.14] | -5.16% / -0.38% | +3.79% / -2.03% | -26.58% / -27.72% | +2.91% / +2.96% |
| field-full-1 | +29.16% [+5.31, +67.97] / +39.52% [+4.05, +58.39] | +51.81% / +61.03% | -- | -7.08% / +15.83% | +5.49% / +5.41% |
| field-full-2 | +50.69% [+31.12, +73.40] / +32.56% [+8.74, +49.50] | +64.99% / +58.82% | +82.89% / +40.67% | -27.95% / -14.68% | +5.56% / +5.72% |
| field-full-16 | +7.10% [-14.43, +32.46] / +2.63% [-10.27, +12.99] | +31.64% / +67.53% | +6.86% / -0.58% | -54.29% / -35.89% | +5.58% / +5.61% |
| method-changed-2 | -3.20% [-11.46, +10.79] / +2.61% [-5.05, +21.90] | -0.51% / +13.20% | -5.43% / -0.25% | -53.08% / -38.00% | +3.54% / +3.61% |
| method-full-w4 | +30.77% [+15.37, +38.12] / +9.20% [-12.89, +44.31] | +40.90% / +55.89% | +29.22% / +6.17% | -51.07% / -30.84% | -0.58% / +1.69% |

### vector-no-promotion-to-vector

| Workload | Lifecycle (95% interval) | First stage | Repeated stage | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| wide | -71.83% [-72.81, -70.30] / -71.64% [-72.28, -71.09] | +89.98% / +94.19% | -80.42% / -80.55% | -15.67% / -1.00% | +76.56% / +76.83% |
| method-full-2 | -0.95% [-10.18, +18.91] / +15.82% [-3.05, +19.62] | +3.19% / +6.51% | -3.85% / +18.43% | -23.23% / -18.27% | -5.00% / -5.02% |
| method-full-16 | -6.04% [-17.56, +1.05] / -9.98% [-28.72, +4.71] | -1.24% / -2.12% | -6.16% / -12.53% | -18.80% / -36.18% | -4.64% / -5.00% |
| low-id-w4 | +1.51% [-10.58, +12.70] / +2.10% [-10.53, +8.14] | +6.13% / -3.46% | +1.14% / +2.25% | -21.28% / -19.37% | +0.25% / +0.17% |

## What changed and what remains costly

Method and field domains convert independently. Sparse misses only request
conversion; the next top-level admission drains existing operations and excludes
warmup. It discards the hash/deque/strings before allocating a fixed vector of
empty strings and ready bytes. Requested values are then regenerated lazily.
One API followed by close avoids an unused conversion. A later unrelated API
can still pay maintenance for a pending domain. There is no allocation-failure
rollback or opaque ownership extension for native Beans.

The normal sparse allocation ledger counts deque requests under its existing
shard lock, not on every hit. The whole-domain rule excludes external character
buffers because they may be rebuilt. It requests conversion at 28681 sequential
IDs in the host 60000-member component case (1500928 structural bytes versus
1500000 dense base). The threshold is ABI-dependent, not the previous 449-ID
pointer-index threshold. Low unique coverage can remain sparse indefinitely.

The normal vector getter has one ready acquire and computes a 24-byte string
object address. The pointer hybrid has two acquires plus a string-pointer lookup.
Actual exported getters contain 22 versus 26 instructions, with old dense at 13.
The real text Bean consumers also contain the new public borrowing check; total
method instruction counts are 86/76/67 for vector/hybrid/old dense. Their counts
do not quantify the hot loop alone. The measurements demonstrate aggregate
behavior, not exact cycle attribution to a removed load. No post-batch timing
profile or additional optimization was added to this fixed comparison.

Normal cache object sizes are 7304 bytes on desktop, 6536 on Android arm64/x86_64
and 2756 on Android armv7/x86. Normal and diagnostic size mirrors match across
all five ABIs. The desktop cache has 624 more fixed bytes per DEX than hybrid.
Normal shared library sizes are:

| Variant | Bytes |
| --- | --- |
| control | 1047904 |
| hybrid | 1066896 |
| vector | 1068912 |
| vector-no-promotion | 1068624 |

## Validation, review and reproduction

All 219 native driver commands, four added worker checks, six phase snapshots,
72 workload smokes, 36 diagnostic native runs and twelve QQ full-oracle runs
passed. Required Core/JAR/JVM/AAR tasks passed with 71 JVM tests, zero skips and
all four Android ABI libraries; dependency installation and final docs build
passed. Host ASan/UBSan disables leak detection. Android execution/performance
and QAux reflection/hooks are not the acceptance oracle; query outputs and
control flow match the frozen baseline.

The complete [Pro review](VECTOR-DESCRIPTORS-REVIEW.md) read the fixed 4f7a82c
increment. Local follow-through added worker-returned bare Bean retention for
both task templates, clarified context/wait contracts, and captured first-stage
generations. Existing diagnostic source proves old allocations are reclaimed.
The complete review did not independently execute tests or recompute this final
performance data. Development failures and their corrections remain archived.

Enabling this experiment changes direct native Bean borrowing requirements.
The session must cover every view use and worker/destructor completion. An
owner-only context neither extends lifetime nor detects all stale reuse. Do not
hold a session while joining an independently admitted API on that same bridge;
share its context for low-level workers or release it before independent calls.
Java/Kotlin APIs manage their scopes and return owned serialized bytes.

Partial `entrant_wait_ns` sums exclude other waits and can overlap across
entrants. They are never added to wall latency. Four-caller first stages can
mix sparse/dense generations; only w1/r1 proves a single API followed by close.
The diagnostic changed-set case confirmed that its first 30000 IDs actually
crossed the host threshold and only the new 30000 IDs were rebuilt.

The [evidence manifest](evidence/vector-descriptors/v1/manifest.json) identifies
the verified raw archive, ordered samples, source/harness versions, commands,
oracles, assembly, ABI and generation evidence. APK/DEX inputs, QQ corpus and
compiled native/JVM/Android artifacts are omitted; obtain them separately using
the recorded inputs and the [benchmark instructions](README.md). The archived
drivers contain the measured host paths; adjust their ROOT/BASE/E/JDK consistently
when reproducing. Start from the recorded 4f7a82c engine and d078369 harness
snapshots, not an evolving branch head.

This completes the bounded [execution plan](VECTOR-DESCRIPTORS-EXECUTION.md).
Keep both the demonstrated lookup improvement and the cold/output/physical-peak
costs when deciding whether this optional backend fits a workload.
