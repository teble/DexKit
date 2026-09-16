# Direct slot-array publication: completed results

Retain this single change for the measured wide-lookup lifecycle benefit.
It is a limited improvement within the experimental hybrid cache;
HYBRID_DESCRIPTORS remains OFF by default. Wide lookup is the only one of
18 before/after lifecycle comparisons whose improvement interval excludes
zero in both phases. The remaining regression against old dense is still
large, and sparse scattered output has a small repeated physical-peak cost.

## Fixed comparison

Before: `56c680c68f44987979c96f01ccd461a546a1e5cd` hybrid. After: `33b12240934d25192c5ef464d80a60019fd7dec6` hybrid.
Old dense reference: `bf9cee531da6a5d4f237398350eed05d7fcf96c5`, with hybrid disabled.
Other Small14 options, compiler settings and public workloads are identical.
The source change publishes the existing atomic slot-array address directly.
DenseIndex ownership, both allocations, payload, field layout, conversion
cost rule, acquire/release ordering and the preceding fast/Cold split remain.

Frozen plan SHA256: `81012f1020d3e2c87b7672f8d2d9db2db9df8642bae488767cf64b35bf8762d5`.
All 52 sweeps / 624 fresh processes completed: six balanced AB/BA pairs per
case in a main phase and one independently seeded confirmation. Eighteen
cases compare before/after and eight provide direct old-dense residuals.
Every successful sample is retained, with zero exclusions; all frozen
input hashes reconciled before and after measurement. No builds, correctness
runs, profiling or evidence compression ran during the timed comparisons.

Negative percentages favor after. Reported percentages are paired median
changes, not ratios of absolute medians; paired bootstrap 95% intervals are
exploratory per-case estimates with six pairs and no multiple-comparison
adjustment. Values below are main / confirmation. Lifecycle includes create,
setup, APIs, result destruction and close. One/four calling-thread cases do
different total work. OS caches were not flushed. These are macOS arm64
measurements, not Android runtime results. Full intervals and absolute
medians are in the [summary](evidence/hybrid-descriptor-direct-slots/v1/summary.json).

## What the measurements support

- Wide lookup lifecycle: -4.72% [-8.30, -2.07] / -9.62% [-13.47, -0.25]. Both phases
  support a benefit for this workload, with differing effect sizes. The
  repeated API stage changes -7.75% [-12.22, -2.79] / -11.47% [-13.85, +0.53]; its
  confirmation interval slightly crosses zero. Do not turn the lifecycle
  result into a separately confirmed warm-stage or exact load-cycle claim.
- The direct old-dense wide residual remains +58.13% / +62.19%
  for lifecycle and +73.98% / +78.17% for repeated APIs.
  Direct publication does not make hybrid equivalent to the original array.
- Prefix lookup improves -3.40% / -3.80% and hot narrow lookup -4.94% /
  -1.61% at the lifecycle point estimates, but each confirmation interval
  crosses zero. Both still regress against old dense. Their direction is
  promising without a confirmed general lookup benefit from this batch.
- Full SSO, long output, sparse distributions, concurrent calls and immediate
  close at 448/449/450 members per domain have no two-phase lifecycle result
  excluding zero. No before/after lifecycle regression is established in
  both phases either. These intervals do not prove equivalence or no cost.
- Full SSO at two repeats still costs +9.58% / +7.57% lifecycle versus old
  dense, with first APIs +14.58% / +15.06%, and higher close/peak costs.
  At 16 repeats lifecycle points are +2.46% / +2.60%, but the main interval
  crosses zero. Keep that uncertainty; this change does not remove initial
  string construction, conversion or destruction work.
- QQ and the one-member-per-domain hotspot have no confirmed before/after
  lifecycle gain. They do not convert, so their time changes cannot be
  attributed to the removed dense load. QQ retained its direct old-dense
  peak advantage: -9.14% to -8.19% across the six phase/case results.
  All six peak intervals exclude zero; lifecycle benefits versus old dense
  are not consistently established by both phases in this batch.
- Sparse scattered output has physical-peak changes +0.92% [+0.26, +1.24] / +0.46% [+0.20, +1.05].
  Both intervals are adverse even though requested layouts and object sizes
  match. Absolute peak medians differ by about 48 / 40 KiB. There is no
  measured allocation attribution for this increase; retain it in the
  verdict rather than declaring memory universally unchanged.

For scale, wide before/after lifecycle medians are
20.484 / 19.122 ms in the main phase and
19.914 / 18.179 ms in confirmation.
The separate direct old-dense/after repeated-API medians are
9.333 / 15.952 ms and
9.344 / 16.085 ms.
Do not multiply percentage changes from separate batches to reconstruct
either comparison. No further repeats or threshold changes were added.

## QQ results

| Comparison / workload | Lifecycle (95% interval), main / confirmation | Peak footprint, main / confirmation |
| --- | --- | --- |
| before-to-after-qq-p1-w4 | +1.31% [-0.65, +2.39] / -3.25% [-10.13, +0.72] | -0.11% / +0.13% |
| dense-to-after-qq-p1-w4 | -3.31% [-8.35, +4.58] / -0.49% [-2.78, +0.67] | -8.74% / -8.44% |
| before-to-after-qq-p11-w4 | +0.28% [-4.49, +3.33] / +0.27% [-4.45, +3.35] | -0.13% / +0.04% |
| dense-to-after-qq-p11-w4 | -0.14% [-2.14, +0.54] / -0.27% [-3.27, +9.21] | -8.87% / -8.19% |
| before-to-after-qq-p1-w1 | -0.02% [-2.47, +1.96] / -0.08% [-1.71, +2.32] | -0.21% / -0.13% |
| dense-to-after-qq-p1-w1 | -1.57% [-5.97, +0.60] / -1.63% [-7.75, -0.75] | -9.14% / -8.94% |

## Native results

Stage/peak columns show paired median changes; full intervals and absolute
values remain in JSON. A dash means the repeated stage is absent or zero.

### before-to-after

| Workload | Lifecycle (95% interval) | First APIs | Repeated APIs | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| dense-output-sso-r2 | -3.25% [-6.36, -0.35] / -0.40% [-0.84, +0.26] | -0.64% / -0.81% | -1.06% / -0.04% | -6.29% / +12.19% | -0.31% / +0.08% |
| dense-output-sso-r16 | -0.79% [-1.09, +6.67] / -0.23% [-0.87, +1.55] | -1.56% / +0.25% | -0.19% / -0.43% | -16.97% / -9.73% | -0.06% / +0.53% |
| dense-output-sso-prefix-r16 | -1.60% [-5.83, +5.81] / +0.54% [-0.40, +1.35] | +4.38% / +4.40% | -1.54% / +0.66% | -20.72% / -16.63% | +0.65% / +0.52% |
| dense-output-sso-scattered-r16 | -0.26% [-2.92, +3.74] / +1.31% [-0.89, +2.77] | +2.86% / +1.46% | -0.82% / +1.19% | +21.03% / +12.94% | +0.92% / +0.46% |
| dense-output-sso-shard-r16 | -1.53% [-2.32, +6.37] / -0.09% [-5.53, +3.65] | -5.15% / +2.91% | -0.53% / -1.30% | +5.17% / -0.63% | -0.33% / +0.20% |
| symbol-output-r16 | -0.84% [-2.05, +3.38] / +0.40% [-8.65, +4.83] | -4.35% / -0.38% | -0.23% / -0.06% | +0.99% / -10.33% | +0.09% / -2.66% |
| symbol-lookup-r512 | -4.72% [-8.30, -2.07] / -9.62% [-13.47, -0.25] | +5.29% / -6.19% | -7.75% / -11.47% | +0.49% / -0.44% | -0.08% / +0.08% |
| overload-lookup-prefix-r256 | -3.40% [-4.96, -2.80] / -3.80% [-4.74, +1.20] | +3.02% / +3.35% | -4.17% / -4.27% | +8.61% / -14.15% | +0.14% / -0.14% |
| symbol-lookup-hot-r100000 | -4.94% [-14.03, -1.55] / -1.61% [-2.72, +0.86] | -2.05% / +2.38% | -5.09% / -1.80% | +6.13% / +2.90% | +0.45% / +0.00% |
| symbol-lookup-concurrent-w1-r20000 | -6.41% [-13.14, +1.80] / +7.14% [-1.98, +23.38] | +2.78% / -27.98% | -6.36% / +7.56% | +0.67% / +3.30% | +0.47% / -1.39% |
| symbol-lookup-concurrent-w4-r20000 | -3.37% [-11.17, +9.51] / +3.90% [-15.73, +6.96] | +21.37% / -6.69% | -3.42% / +3.92% | -3.23% / -5.10% | +2.25% / +0.00% |
| dense-output-sso-shard-r1-k448 | -0.58% [-17.77, +0.91] / -2.68% [-4.14, +7.30] | +0.29% / +1.26% | -- | -3.77% / -2.12% | +0.77% / +0.28% |
| dense-output-sso-shard-r1-k449 | +1.10% [-2.77, +9.34] / -0.45% [-4.99, +7.56] | -4.78% / -6.80% | -- | -12.45% / +0.16% | +0.56% / +0.49% |
| dense-output-sso-shard-r1-k450 | -9.32% [-17.51, +9.28] / +0.23% [-3.51, +4.51] | -8.74% / -1.06% | -- | -32.43% / +69.30% | +0.28% / +0.21% |
| dense-output-sso-shard-r100000-k1 | +0.54% [-1.24, +1.83] / +0.52% [-1.77, +2.02] | -1.49% / +0.86% | +0.85% / +0.28% | +7.31% / +5.57% | +0.21% / +0.07% |

### dense-to-after

| Workload | Lifecycle (95% interval) | First APIs | Repeated APIs | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| dense-output-sso-r2 | +9.58% [+3.34, +11.41] / +7.57% [+6.20, +9.14] | +14.58% / +15.06% | +7.40% / +2.78% | +60.92% / +75.88% | +7.57% / +5.59% |
| dense-output-sso-r16 | +2.46% [-15.28, +3.71] / +2.60% [+2.11, +3.50] | +12.93% / +16.39% | +1.82% / +1.40% | +108.01% / +69.24% | +5.46% / +5.30% |
| symbol-lookup-r512 | +58.13% [+47.09, +63.04] / +62.19% [+52.99, +66.25] | +5.85% / +6.56% | +73.98% / +78.17% | +1.31% / +39.95% | +1.29% / +1.29% |
| overload-lookup-prefix-r256 | +14.46% [+13.02, +16.71] / +14.42% [+13.60, +16.50] | +0.96% / +6.97% | +15.81% / +14.92% | +1.69% / +8.23% | +1.05% / +0.98% |
| symbol-lookup-hot-r100000 | +4.74% [+4.06, +5.47] / +5.08% [+4.53, +5.71] | +219.09% / +197.81% | +5.00% / +5.21% | -1.12% / -7.82% | -5.08% / -5.06% |

## Mechanism and unchanged storage

Normal dylib method/field descriptor getters each change 27 -> 26 instructions
and keep both acquire loads. The timed executable inlines the same probe:
method text lookup changes 77 -> 76 instructions, field 67 -> 66, by removing
the ordinary owner-to-slots read following the first acquire. The method
lookup frame remains 160 bytes. Numeric Bean consumers also lose that load.
The Cold branch, string access, candidate order and comparison remain.

This is a more direct mechanism comparison than the preceding entry-only
change, whose exported getter frame was not present per candidate in the
inlined lookup loop. It still measures the whole compiled source change;
scheduling, address layout and noise prevent exact cycle attribution to a
single instruction. The previous post-batch profile is historical evidence,
not a new profile of this source version.

Sixteen paired diagnostic cases reconcile 4352 tables: completed calls,
hits, records, conversions and requested capacities match the previous
hybrid version. Race-dependent lock-free dense_hits and conversion duration
are excluded only from this diagnostic equality comparison, not from timing
sample acceptance. QQ has 1628 method records, zero field records and zero
conversions. The 448-per-domain case remains sparse; 449 converts both
method and field domains once and frees their buckets; 450 does not reconvert.

Five ABI probes confirm unchanged normal and diagnostic cache/deque owner
sizes: normal cache 6680 bytes on desktop, 5912 on Android arm64/x86_64, and
2444 on Android armv7/x86. Diagnostic allocator requests and partial
conversion overlap are not a decomposition of normal physical peaks.

## Validation and review

All 184 native driver commands, 42 normal workload smokes and four QQ full
oracle runs passed before timing. Core/JAR/JVM/AAR tasks passed with 71 tests,
zero skips and all four Android ABI libraries. Existing checks cover the
new array publication, retained SSO/long/empty views, delayed sparse readers,
concurrent first insertion and bounds. Host ASan/UBSan disables leak
detection; latch hooks are diagnostic-only. Android runtime is not covered.

The complete [Pro review](HYBRID-DESCRIPTOR-DIRECT-SLOTS-REVIEW.md) read
`eed75a8 -> 33b1224` and the relevant unchanged consumers/checks. It found
no confirmed ownership, boundary, publication or macro blocker. Its reporting
limits are adopted; Pro did not inspect these binaries, rerun tests or
independently recompute the final performance statistics.

The [evidence manifest](evidence/hybrid-descriptor-direct-slots/v1/manifest.json)
records source/artifact identities and a fully verified raw archive with
ordered samples, oracles, commands, assembly and diagnostic/ABI results.
APKs, DEX inputs and compiled native/JVM/Android artifacts are omitted.
This completes the bounded [execution plan](HYBRID-DESCRIPTOR-DIRECT-SLOTS.md).
The measured scope supports keeping direct publication within the hybrid
experiment; it does not support enabling hybrid by default or claiming
that the remaining dense-read and full-coverage costs are resolved.
