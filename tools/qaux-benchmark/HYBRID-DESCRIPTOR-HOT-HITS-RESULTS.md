# Hybrid descriptor fast entry: completed results

Do not promote this entry change as a validated general speed optimization.
The fixed source remains an experimental checkpoint for focused follow-up;
HYBRID_DESCRIPTORS remains OFF by default. None of the 18 before/after lifecycle
comparisons has an improvement interval excluding zero in both phases. The
previous conditional storage verdict remains: QQ uses less peak memory, while
full SSO and wide/prefix lookup still regress against the original dense cache.

## Fixed comparison

Before: `bf9cee531da6a5d4f237398350eed05d7fcf96c5` hybrid. After: `56c680c68f44987979c96f01ccd461a546a1e5cd` hybrid.
The old dense reference uses the same baseline source with HYBRID disabled.
All other Small14 options and compiler settings are identical. Ownership,
payload, fixed object layout, conversion rule and publication order are unchanged.

Frozen plan SHA256: `cf4f89263dd692d66c370d397bec6d11742814ab1c0dcae63807c66862d4b31f`.
All 52 planned sweeps and 624 fresh processes completed: six balanced AB/BA
pairs per case in a main phase and one independent confirmation. Eighteen cases
compare before/after; eight compare old dense/after as a direct residual reference.
No successful samples were excluded, and every frozen input hash reconciled.
No builds, correctness tests or profiling ran during the formal comparisons.
Profiling and extra call-site inspection below ran only after the batch ended.

Percentages favor the second label when negative and are paired median changes,
not ratios of separately reported absolute medians. Values are main / confirmation.
Lifecycle includes create, setup, APIs, result destruction and close. One/four
calling-thread cases execute different total work and are separate comparisons.
OS caches were not flushed. Bootstrap 95% intervals are exploratory per-case
estimates without a multiple-comparison adjustment. All absolute medians and
intervals are in the [summary](evidence/hybrid-descriptor-hot-hits/v1/summary.json);
ordered individual samples and logs are in the raw evidence archive.

## What the measurements support

- Wide lookup does not improve: before/after lifecycle +1.85% / +1.55%, both
  intervals crossing zero. Repeated API changes are +1.70% / -0.86%, also
  inconclusive. The direct old-dense/after lifecycle residual is +66.63% /
  +70.95%, with repeated APIs +88.22% / +94.58%.
- Full SSO at 16 repeats is essentially unchanged against before (-0.32% /
  -0.01% lifecycle), and still +3.21% / +3.33% against old dense. At two
  repeats, the old-dense residual is +8.11% / +10.31%; first APIs, close and
  physical peak remain worse. This entry change does not remove construction
  or destruction work.
- Prefix lookup has small before/after changes (-0.38% / -0.74% lifecycle),
  without a two-phase lifecycle result excluding zero. It still regresses
  +19.07% / +18.97% against old dense.
- Hot narrow lookup changes -1.17% / -0.66% over the lifecycle. The main
  interval excludes zero, but confirmation includes a retained large outlier:
  pair 0 after takes 141.674 ms versus 32.500 ms before, chiefly in repeated
  APIs. The confirmation interval is [-1.13%, +167.84%]. The ordinary median
  shift alone is not a robust confirmed gain.
- QQ before/after lifecycle directions do not replicate consistently. Its
  direct peak advantage versus old dense remains -8.14% to -9.04% in this
  batch. QQ has zero conversions, so any entry-time change is not a gain from
  dense hits. No repeated cold/QQ lifecycle regression is established either.
- The one-member-per-domain first call improves about 19% in both phases,
  but the absolute difference between medians is about 1.46 microseconds.
  Its complete lifecycle changes -0.44% / -0.90%, with the main interval
  crossing zero. Preserve this small stage observation without calling it a
  broad speedup.
- Physical peak is not uniformly unchanged even though diagnostic requested
  layouts match. Sparse prefix and single-shard output increase about 1.2%
  in both phases, roughly 0.1 MiB. Those adverse observations remain in the
  decision; there is no measured allocation attribution that explains them.
- Concurrent and immediate-close results remain noisy; all their lifecycle
  intervals cross zero in at least one phase. No extra confirmation, tuning or
  outlier removal was added to force a verdict.

For scale, the direct old-dense/after wide repeated-API medians are
8.970 / 16.464 ms in the main phase and
8.676 / 17.006 ms in confirmation.
Full SSO at 16 repeats has old-dense/after close medians of
0.225 / 0.313 ms and
0.161 / 0.326 ms, within roughly 139-150 ms complete lifecycles.

## QQ results

| Comparison / workload | Lifecycle (95% interval), main / confirmation | Peak footprint, main / confirmation |
| --- | --- | --- |
| before-to-after-qq-p1-w4 | +1.11% [-1.55, +3.37] / -0.85% [-2.53, +12.42] | +0.15% / +0.18% |
| dense-to-after-qq-p1-w4 | -1.97% [-3.59, +0.53] / -1.59% [-5.62, +1.65] | -8.87% / -9.04% |
| before-to-after-qq-p11-w4 | +0.41% [-1.74, +1.88] / -0.77% [-4.95, -0.03] | +0.33% / -0.29% |
| dense-to-after-qq-p11-w4 | -2.41% [-8.59, -0.29] / -1.73% [-3.09, -0.76] | -8.91% / -8.14% |
| before-to-after-qq-p1-w1 | +0.54% [+0.07, +3.08] / -0.29% [-1.77, +0.16] | +0.13% / +0.08% |
| dense-to-after-qq-p1-w1 | -1.07% [-3.17, -0.63] / -1.00% [-1.66, -0.12] | -8.70% / -8.83% |

## Native results

Stage and peak columns give paired median changes; full intervals and absolute
values are in JSON. A dash means the repeated interval is absent or zero.

### before-to-after

| Workload | Lifecycle (95% interval) | First APIs | Repeated APIs | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| dense-output-sso-r2 | +1.24% [-1.34, +6.11] / +0.63% [-1.05, +1.99] | +0.75% / +0.58% | +6.11% / +0.59% | -7.91% / -7.06% | +0.41% / +0.27% |
| dense-output-sso-r16 | -0.32% [-1.10, +0.42] / -0.01% [-1.56, +0.81] | -1.47% / +2.24% | -0.62% / -0.44% | +5.91% / +31.64% | +0.25% / -0.00% |
| dense-output-sso-prefix-r16 | +1.57% [-2.11, +4.32] / -0.47% [-1.48, +1.90] | -2.41% / +0.57% | +2.62% / +2.00% | -4.21% / -12.94% | +1.19% / +1.20% |
| dense-output-sso-scattered-r16 | +0.62% [-2.41, +3.87] / +1.22% [-0.11, +6.87] | -1.47% / +7.98% | +0.61% / +2.08% | +3.30% / +25.37% | +0.33% / +0.60% |
| dense-output-sso-shard-r16 | -1.10% [-3.49, +1.37] / +1.00% [-1.25, +2.64] | -2.86% / +6.38% | -1.24% / -0.63% | -26.11% / +0.25% | +1.15% / +1.16% |
| symbol-output-r16 | -0.70% [-13.79, +2.55] / -1.12% [-2.58, +1.36] | -4.33% / -4.97% | -2.70% / -0.67% | -10.93% / +16.42% | +0.20% / +0.29% |
| symbol-lookup-r512 | +1.85% [-26.67, +17.36] / +1.55% [-9.08, +2.99] | +17.31% / +2.05% | +1.70% / -0.86% | +8.71% / -5.04% | -0.08% / -0.08% |
| overload-lookup-prefix-r256 | -0.38% [-3.79, +6.35] / -0.74% [-4.40, +0.04] | +1.08% / -2.63% | -0.40% / -0.81% | -12.58% / -5.36% | -0.28% / -0.28% |
| symbol-lookup-hot-r100000 | -1.17% [-1.71, -0.57] / -0.66% [-1.13, +167.84] | +1.33% / +1.11% | -1.04% / -0.62% | +2.90% / +12.00% | +0.00% / -0.45% |
| symbol-lookup-concurrent-w1-r20000 | -2.06% [-25.58, +6.64] / +0.38% [-21.57, +5.44] | +24.78% / +5.11% | -2.32% / +0.47% | +2.31% / -10.92% | +0.47% / +1.89% |
| symbol-lookup-concurrent-w4-r20000 | -11.72% [-17.54, +7.06] / +5.41% [-8.24, +30.91] | +15.37% / -26.05% | -11.80% / +5.49% | -15.92% / -14.71% | +1.83% / +1.37% |
| dense-output-sso-shard-r1-k448 | -7.75% [-9.66, +0.85] / +0.14% [-2.02, +1.07] | +0.90% / -0.62% | -- | -23.26% / +19.50% | -0.07% / -0.07% |
| dense-output-sso-shard-r1-k449 | -0.08% [-9.66, +5.35] / +3.65% [-2.50, +11.12] | +0.44% / +9.88% | -- | -17.78% / +56.31% | -0.21% / -0.07% |
| dense-output-sso-shard-r1-k450 | +8.60% [-4.54, +12.89] / +0.18% [-2.52, +8.00] | +2.45% / +5.25% | -- | +3.89% / +7.26% | -0.21% / -0.07% |
| dense-output-sso-shard-r100000-k1 | -0.44% [-1.97, +1.18] / -0.90% [-1.82, -0.41] | -18.69% / -19.02% | -0.50% / -0.80% | -14.26% / -25.46% | +0.07% / -0.14% |

### dense-to-after

| Workload | Lifecycle (95% interval) | First APIs | Repeated APIs | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| dense-output-sso-r2 | +8.11% [+4.07, +12.46] / +10.31% [+8.61, +45.95] | +16.01% / +20.21% | +2.46% / +7.27% | +58.68% / +159.77% | +5.33% / +5.59% |
| dense-output-sso-r16 | +3.21% [+1.88, +4.06] / +3.33% [+2.84, +3.71] | +13.56% / +15.67% | +2.47% / +2.23% | +23.77% / +101.89% | +5.57% / +5.53% |
| symbol-lookup-r512 | +66.63% [+57.60, +72.04] / +70.95% [+60.23, +79.65] | +3.83% / +0.83% | +88.22% / +94.58% | +18.30% / +10.64% | +1.38% / +1.20% |
| overload-lookup-prefix-r256 | +19.07% [+16.66, +23.76] / +18.97% [+17.76, +20.56] | +5.80% / +1.47% | +19.79% / +19.83% | +6.90% / +6.98% | +1.05% / +0.98% |
| symbol-lookup-hot-r100000 | +5.86% [+5.13, +6.88] / +5.50% [+5.25, +6.04] | +220.18% / +199.95% | +6.32% / +5.88% | +4.00% / +0.55% | -4.24% / -5.06% |

## Mechanism checked after timing

The standalone normal getters do shrink from 38 to 27 instructions and their
successful paths no longer create a frame. Inspecting the actual timed native
executables reveals the important limit: method and field text lookup inline
the getter. The method lookup function keeps its existing 160-byte frame;
its loop loses two callback-capture stores, not a per-candidate function frame.
The complete method lookup changes 80 -> 77 instructions and field lookup
72 -> 67, while their dependent dense-index reads remain. Index-based Bean
getters also inline the probe. A shorter exported getter is not evidence that
all those call sites have lost call/return or frame work.

A one-pass versus 512-pass diagnostic lookup has the same 4096 cached records
and 32 conversions. All additional 4186623 cache calls are lock-free dense hits:
zero extra construction, promotion or requested payload/index capacity. This
isolates the descriptor cache warm path; it does not claim that the complete
GetMethodData wrapper avoids all hashes, locks or output allocations.

One-second statistical samples of the normal before/after/old-dense executables
at 100000 lookup repeats place 657/838, 644/840 and 389/789 main-thread top-of-stack
samples in the text GetMethodBean loop, respectively. The remaining prominent
work includes memcmp. These samples locate a hot region, not an exact cycle
attribution to one load; their perturbed runtimes are excluded from all 624
formal samples. The call-site assembly and complete sample reports are archived.

The next bounded hypothesis is to publish the slot-array pointer directly,
while retaining the same DenseIndex owner, allocations, byte rule, shard layout
and acquire/release protocol. That would remove one dependent owner-to-slots
load without combining a payload rewrite or threshold change. It is not
implemented or measured by this batch, and this observation does not establish
its benefit.

## Validation and review

All 184 native driver commands, 42 normal workload smokes and four QQ full
oracle runs passed. The Core/JAR/JVM/AAR Gradle tasks passed with 71 tests, zero
skips and four Android ABI libraries. Host ASan/UBSan disabled leak detection;
Android runtime performance is not covered. Sixteen diagnostic before/after
cases reconcile 4352 tables and completed-access counters. Requested capacities
are diagnostic observations, not a decomposition of normal physical peaks.

The complete [Pro source review](HYBRID-DESCRIPTOR-HOT-HITS-REVIEW.md) covered
`e454b24 -> 56c680c` and found no confirmed correctness blocker. Its limitations
are adopted: dense_hits does not count all dense hits or factories avoided;
component helper tests verify semantics, while the actual compiled call sites
and public API experiments evaluate entry costs. Pro did not rerun the tests,
disassemble the binaries or independently recompute these final measurements.

The [evidence manifest](evidence/hybrid-descriptor-hot-hits/v1/manifest.json)
contains hashes and a verified raw archive with inputs/commands, source exports,
ordered byte oracles, normal/diagnostic results, assembly and post-batch profiles.
APKs, DEX inputs and compiled native/JVM/Android artifacts are omitted; their
identities and baseline source are recorded. This report completes the bounded
work recorded in the frozen [execution plan](HYBRID-DESCRIPTOR-HOT-HITS.md).
