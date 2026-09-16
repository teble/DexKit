# Hybrid descriptor cache: fixed-rule results

Keep the hybrid as a conditional experiment. It preserves the measured QQ memory
advantage and reduces high-coverage hash/lock cost relative to the new sparse-only
cache. Full-coverage construction/close costs and residual text-lookup regressions
against the original dense array prevent enabling it by default. The sparse-only
variant remains an attribution control; its larger flat buckets and repeated
lookup costs do not support general adoption over the existing node cache.

## Reproduction and scope

Engine: `bf9cee531da6a5d4f237398350eed05d7fcf96c5`. Plan SHA256:
`c7a9e7b6a81cd0750f1efa3679dd10100168a65a9eb55e06426717b08db3fb19`.

All 108 planned sweeps and 1296 fresh processes completed, with no excluded
successful measurements, input changes, native builds or correctness runs during
measurement. Each case has six balanced AB/BA pairs in a main phase and six in
one independently seeded confirmation. Compiler, deployment target and all other
Small14 options were held fixed. Packed cross identities and packed field uses
were OFF. Both new descriptor flags remain OFF by default.

Four builds are covered by three comparisons: old dense -> hybrid is the net
effect; node -> sparse isolates the new payload/flat-index route; sparse -> hybrid
isolates conversion and its state check. Separate pairs are not a shared
four-way sample: do not multiply their median deltas into an inferred comparison.

Negative percentages favor the second build. Values below are main / confirmation.
Lifecycle includes create, setup, API work, result destruction and close; it is
not the sum of independently rounded stage medians. Repeated APIs are the whole
repeated group. One/four calling-thread cases perform different total work and
are compared independently. File caches were not flushed. Percentile bootstrap
intervals are exploratory per-case estimates, without a multiple-comparison
adjustment. All absolute medians, intervals and raw samples are preserved in the
[evidence summary](evidence/hybrid-descriptors/v1/summary.json).

## Decision evidence

- QQ peak footprint changes are -8.78% to -9.22% across the three workloads and
  two phases. Close is consistently lower. Several lifecycle/API intervals cross
  zero, so this is principally a measured memory benefit, not a broad speed claim.
- Full SSO output at 16 repeats improves by 24.03% / 28.10% against sparse-only;
  its physical peak falls by 10.06% / 10.30%. It remains 3.92% / 3.10% slower
  than old dense, with 4.55% / 3.73% higher physical peak. At two repeats the
  lifecycle increase is 7.97% / 7.50%, and first APIs are about 14.7% / 14.5%
  slower. The main two-repeat lifecycle interval crosses zero; confirmation does not.
- Wide text lookup improves by 64.04% / 62.66% against sparse-only, but still
  regresses 70.13% / 65.81% against old dense. Repeated APIs regress 92.38% /
  85.81%. Prefix lookup remains about 18.9% / 18.6% slower over the lifecycle.
- New sparse-only full SSO at 16 repeats regresses 8.55% / 5.78% against node,
  and its peak increases 3.30% / 5.03%. Fewer per-node frees do not compensate
  for the larger hash and repeated lookup cost in that case.
- Sparse prefix/scattered output can have slower first/repeated APIs even when
  reduced creation/close work improves the lifecycle. Do not describe the route
  as regression-free for sparse workloads.
- One member per domain never converts. Its 100000-repeat lifecycle changes
  are +2.82% / +2.89% against old dense; the main interval crosses zero and the
  confirmation interval is positive. Concurrent shared-hotspot results are much
  noisier: the one-thread main regression does not replicate at the same scale,
  and four-thread intervals span zero. These do not establish a general win.
- Immediate close at 448/449/450 members per domain preserves a large peak
  reduction against old dense, but isolated conversion latency is not free or
  reliably amortized. Several first/lifecycle intervals are wide. The long-output
  confirmation also has a large positive outlier; it is retained in the raw data.

Close percentages need absolute scale. In the 16-repeat full SSO net comparison,
old dense / hybrid close medians are 0.180 / 0.358 ms in the main phase,
and 0.178 / 0.343 ms in confirmation. The percentage increase is real in this
fixture but is a small part of its roughly 140-150 ms complete lifecycle.

## QQ paired results

Lifecycle values include their 95% paired bootstrap interval in brackets.
Peak values are paired median percentage changes, with full intervals in JSON.

| Comparison / workload | Lifecycle main / confirmation | Peak footprint main / confirmation |
| --- | --- | --- |
| control-to-hybrid-qq-p1-w4 | -2.84% [-5.59, +0.62] / -4.33% [-11.51, -2.44] | -8.85% / -9.17% |
| node-to-sparse-qq-p1-w4 | -0.62% [-1.61, +2.21] / +1.07% [-5.50, +3.92] | -0.04% / +0.53% |
| sparse-to-hybrid-qq-p1-w4 | -2.17% [-7.04, +3.89] / -0.02% [-2.69, +2.29] | -0.27% / +0.15% |
| control-to-hybrid-qq-p11-w4 | -0.38% [-1.20, +0.58] / -1.44% [-3.36, +0.87] | -9.22% / -8.78% |
| node-to-sparse-qq-p11-w4 | -0.28% [-3.62, +0.57] / -0.37% [-1.78, +0.51] | +0.29% / +0.09% |
| sparse-to-hybrid-qq-p11-w4 | +0.05% [-5.90, +2.93] / +0.23% [-0.50, +3.97] | +0.08% / -0.07% |
| control-to-hybrid-qq-p1-w1 | -1.81% [-2.86, +1.80] / -2.39% [-6.15, -1.08] | -8.97% / -9.01% |
| node-to-sparse-qq-p1-w1 | -0.46% [-3.64, +4.22] / -1.83% [-3.53, -0.11] | +0.54% / -0.05% |
| sparse-to-hybrid-qq-p1-w1 | -0.36% [-2.72, +3.54] / -0.15% [-0.78, +1.46] | +0.01% / +0.17% |

## Native paired results

First, repeated, close and peak columns give main / confirmation median changes.
A dash is an unmeasured/zero repeated interval, including the one-pass boundary
cases. Full CIs and absolute medians accompany every available metric in JSON.

### control-to-hybrid

| Workload | Lifecycle (95% interval) | First APIs | Repeated APIs | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| dense-output-sso-r2 | +7.97% [-20.71, +8.88] / +7.50% [+6.17, +8.61] | +14.73% / +14.45% | +1.45% / +1.35% | +76.10% / +65.64% | +5.04% / +4.96% |
| dense-output-sso-r16 | +3.92% [+3.48, +4.37] / +3.10% [+0.55, +4.65] | +16.91% / +13.61% | +2.91% / +2.55% | +69.26% / +112.94% | +4.55% / +3.73% |
| dense-output-sso-prefix-r16 | +0.98% [-2.51, +2.71] / -1.28% [-3.42, +0.10] | +40.11% / +36.53% | +13.18% / +15.00% | -67.35% / -71.07% | -21.65% / -21.67% |
| dense-output-sso-scattered-r16 | -3.44% [-6.82, -0.76] / -3.96% [-4.62, -2.23] | +20.23% / +11.22% | +10.05% / +5.81% | -59.26% / -58.43% | -21.85% / -22.00% |
| dense-output-sso-shard-r16 | -3.11% [-6.92, -1.85] / -5.28% [-9.77, -3.67] | -3.04% / +7.47% | -0.09% / +1.11% | -62.26% / -67.96% | -23.81% / -23.78% |
| symbol-output-r16 | -4.73% [-7.43, -1.04] / +0.33% [-2.83, +66.50] | +4.33% / -1.79% | -6.75% / +1.22% | +3.85% / -2.23% | -5.11% / -4.94% |
| symbol-lookup-r512 | +70.13% [+57.78, +81.91] / +65.81% [+49.95, +77.61] | -0.32% / +5.47% | +92.38% / +85.81% | +32.90% / +12.51% | +1.46% / +1.20% |
| overload-lookup-prefix-r256 | +18.89% [+17.02, +23.50] / +18.63% [+13.31, +24.35] | +3.80% / +1.59% | +19.45% / +19.40% | +9.13% / +25.05% | +1.33% / +1.12% |
| symbol-lookup-hot-r100000 | +6.80% [+6.11, +7.18] / +6.22% [+4.09, +6.79] | +200.34% / +204.08% | +7.36% / +6.46% | -16.72% / -4.61% | -4.27% / -4.68% |
| symbol-lookup-concurrent-w1-r20000 | +53.85% [+27.76, +60.97] / +3.72% [-2.17, +34.12] | +35.69% / +59.14% | +56.41% / +3.85% | +7.48% / +31.23% | -6.16% / -5.35% |
| symbol-lookup-concurrent-w4-r20000 | +1.94% [-6.51, +18.21] / +1.28% [-17.48, +29.71] | -18.48% / +23.07% | +2.01% / +1.27% | -6.37% / +24.83% | -3.86% / -5.17% |
| dense-output-sso-shard-r1-k448 | -6.71% [-16.91, +0.14] / -4.42% [-13.15, -0.05] | +4.86% / +0.80% | -- | -76.91% / -72.39% | -24.27% / -24.33% |
| dense-output-sso-shard-r1-k449 | -7.20% [-15.61, -0.28] / -13.78% [-39.12, +121.12] | +30.67% / +1.78% | -- | -54.83% / -74.09% | -24.02% / -25.02% |
| dense-output-sso-shard-r1-k450 | -12.70% [-15.46, -5.25] / -3.93% [-10.57, +0.17] | +5.62% / +32.29% | -- | -76.41% / -68.55% | -24.06% / -24.10% |
| dense-output-sso-shard-r100000-k1 | +2.82% [-12.35, +3.67] / +2.89% [+1.71, +7.19] | +29.03% / +18.74% | +4.73% / +4.33% | -79.97% / -77.97% | -24.75% / -24.42% |

### node-to-sparse

| Workload | Lifecycle (95% interval) | First APIs | Repeated APIs | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| dense-output-sso-r2 | -8.68% [-12.61, -3.58] / -8.61% [-9.29, -6.67] | -6.26% / -9.22% | +2.19% / +11.15% | -87.08% / -84.58% | +3.00% / +2.98% |
| dense-output-sso-r16 | +8.55% [+6.56, +10.50] / +5.78% [+2.89, +10.42] | -6.53% / -9.76% | +12.29% / +9.92% | -88.39% / -87.52% | +3.30% / +5.03% |
| dense-output-sso-prefix-r16 | +0.87% [-4.14, +3.97] / +0.23% [-1.13, +2.03] | -2.65% / -0.46% | +1.71% / +0.98% | -45.37% / -29.15% | +3.25% / +2.89% |
| dense-output-sso-scattered-r16 | +2.09% [-2.41, +4.56] / -0.24% [-2.57, +3.27] | +3.84% / -9.90% | +5.02% / +0.60% | -18.92% / -36.40% | +2.89% / +3.16% |
| dense-output-sso-shard-r16 | +0.08% [-3.03, +6.63] / -1.18% [-2.09, +0.17] | -10.06% / -7.52% | +3.58% / +1.31% | -46.10% / -40.86% | +0.68% / +1.02% |
| symbol-output-r16 | -2.66% [-4.63, -0.14] / -1.19% [-7.38, +3.49] | +0.32% / -1.26% | -3.01% / -0.21% | -44.82% / -47.85% | +0.43% / -2.15% |
| symbol-lookup-r512 | +10.66% [+8.32, +11.15] / +7.33% [+4.10, +11.45] | -2.26% / -1.90% | +11.76% / +8.40% | -44.88% / -37.93% | +1.51% / +1.51% |
| overload-lookup-prefix-r256 | +7.43% [+6.10, +11.10] / +9.12% [+6.42, +11.28] | -2.70% / -2.52% | +8.17% / +9.62% | -38.85% / -28.66% | +1.23% / +1.17% |
| symbol-lookup-hot-r100000 | +1.49% [+1.10, +3.15] / +1.12% [-0.51, +2.13] | +107.35% / +94.37% | +1.55% / +1.14% | -0.13% / +3.83% | +2.31% / +1.38% |
| symbol-lookup-concurrent-w1-r20000 | -2.77% [-5.24, +4.78] / -0.16% [-6.19, +16.18] | +41.34% / +7.62% | -2.99% / -0.43% | +16.73% / +1.67% | +6.28% / +5.74% |
| symbol-lookup-concurrent-w4-r20000 | +7.96% [-14.99, +15.34] / -4.23% [-12.86, +10.34] | +14.05% / -32.38% | +8.02% / -4.21% | +10.71% / +28.50% | +4.56% / +5.09% |
| dense-output-sso-shard-r1-k448 | +2.26% [-2.72, +7.99] / +1.61% [-5.62, +2.99] | -10.80% / -16.94% | -- | -29.44% / -38.39% | +0.07% / +0.21% |
| dense-output-sso-shard-r1-k449 | -9.05% [-33.12, +0.17] / +0.81% [-4.36, +1.85] | -7.61% / -10.15% | -- | -15.16% / -43.77% | +0.07% / -0.21% |
| dense-output-sso-shard-r1-k450 | -0.45% [-7.09, +10.07] / -2.77% [-5.43, +2.81] | -17.71% / -9.80% | -- | -50.26% / -43.77% | +0.21% / +0.28% |
| dense-output-sso-shard-r100000-k1 | +0.92% [-0.33, +2.41] / -0.74% [-2.37, +1.38] | +8.69% / +1.48% | +1.23% / -0.41% | +1.23% / +11.26% | +0.43% / +0.07% |

### sparse-to-hybrid

| Workload | Lifecycle (95% interval) | First APIs | Repeated APIs | Close | Peak footprint |
| --- | --- | --- | --- | --- | --- |
| dense-output-sso-r2 | -13.36% [-14.93, -12.63] / -15.26% [-18.66, -12.37] | -10.88% / -10.72% | -23.92% / -26.31% | +0.17% / +3.37% | -9.74% / -9.65% |
| dense-output-sso-r16 | -24.03% [-24.12, -23.67] / -28.10% [-28.63, -26.02] | -11.95% / -12.98% | -26.02% / -30.27% | -32.05% / -6.44% | -10.06% / -10.30% |
| dense-output-sso-prefix-r16 | +1.48% [-0.07, +4.56] / -0.46% [-4.87, +2.83] | +0.22% / -2.28% | -0.26% / +0.24% | +16.48% / -15.88% | -1.50% / -1.05% |
| dense-output-sso-scattered-r16 | +0.41% [-1.97, +6.61] / +2.39% [-0.37, +6.39] | +10.21% / +13.17% | -0.16% / +1.53% | -1.96% / +12.40% | -1.17% / -1.44% |
| dense-output-sso-shard-r16 | -1.40% [-4.16, +0.36] / -6.21% [-7.85, -1.81] | +7.19% / -11.83% | -10.04% / -10.44% | +1.06% / -8.74% | -1.21% / -1.67% |
| symbol-output-r16 | -0.54% [-0.67, +2.29] / -2.78% [-4.15, +2.70] | -8.83% / -10.83% | +1.65% / -0.07% | +14.04% / +15.31% | -0.83% / -5.14% |
| symbol-lookup-r512 | -64.04% [-64.77, -62.69] / -62.66% [-63.96, -61.66] | -8.40% / -11.09% | -67.32% / -65.99% | +4.74% / +10.20% | -2.48% / -2.65% |
| overload-lookup-prefix-r256 | -23.92% [-24.90, -22.50] / -26.43% [-41.37, -19.45] | -2.36% / +0.53% | -24.58% / -26.93% | -1.08% / -31.92% | -1.76% / -1.83% |
| symbol-lookup-hot-r100000 | -18.58% [-19.37, -18.14] / -18.59% [-19.30, -18.21] | +12.39% / +3.70% | -19.70% / -19.72% | -3.90% / +18.13% | +0.90% / +1.35% |
| symbol-lookup-concurrent-w1-r20000 | +2.68% [-2.95, +8.64] / -22.29% [-36.26, +1.65] | +2.75% / +7.59% | +2.94% / -22.78% | +0.56% / -10.10% | -3.65% / -3.63% |
| symbol-lookup-concurrent-w4-r20000 | -12.04% [-23.33, +18.32] / -1.39% [-9.25, +11.02] | -2.58% / -22.49% | -12.10% / -1.35% | +10.08% / -8.58% | -3.93% / -4.31% |
| dense-output-sso-shard-r1-k448 | -0.80% [-3.09, +7.58] / +4.17% [-6.92, +22.34] | +10.37% / +25.30% | -- | -4.79% / +30.77% | +0.07% / -0.21% |
| dense-output-sso-shard-r1-k449 | -4.15% [-6.24, +3.69] / +1.09% [-9.10, +11.28] | +7.36% / +14.34% | -- | +54.19% / -4.57% | +0.14% / +0.07% |
| dense-output-sso-shard-r1-k450 | +3.00% [-1.62, +6.63] / -1.96% [-18.82, +0.06] | +6.64% / +6.59% | -- | -13.01% / +18.55% | +0.14% / +0.07% |
| dense-output-sso-shard-r100000-k1 | -0.36% [-1.00, +3.04] / +0.87% [-1.57, +1.51] | -5.02% / +12.19% | -0.38% / +0.64% | +2.52% / +14.17% | -0.07% / +0.07% |

## Requested descriptor storage

These are diagnostic requested capacities after removing object-only
instrumentation, not a division of normal physical footprint. The old dense/node
layout reference is explicitly reused from `05e5a41`; those cache types and the
fixtures are unchanged. Old dense includes its formerly omitted fixed owners.
The full-coverage reference has the same 120000 strings but a different access
count from the current workload. Only storage is compared here.

| Workload | Old dense | Node | New sparse-only | Hybrid |
| --- | ---: | ---: | ---: | ---: |
| QQ (1628 methods, zero fields) | 135348661 B | 489457 B | 3970593 B | 3970593 B |
| Full SSO (60000 methods + 60000 fields) | 3962112 B | 6204440 B | 7607320 B | 4111896 B |

QQ uses 835 deque owners and no dense domain. Its diagnostic blocks request
3406800 B, plus 6680 B in block directories, 40080 B in normal-size deque owners,
196369 B in external characters, 46784 B in hash buckets and 273880 B in fixed
cache objects. This trades more sparse storage than node for the hybrid route.

Full SSO converts all 64 domains. Sparse buckets fall from 4455936 B to zero;
dense indexes request 960512 B. Both new variants retain the same 3133440 B of
string blocks, 8192 B in block directories and 3072 B of normal-size deque owners.
Block slack is 253440 B above the 120000 24-byte strings. There are no external
character allocations in this SSO fixture. Hybrid is still 149784 B larger than
the old dense layout; normal process peaks, including allocator effects, show a
larger difference than that requested-byte delta.

Each 1875-slot host domain promotes at 449 entries: hash requests jump from
8696 B to 17400 B, crossing the 15008 B dense cost. The API boundary workload
selects the same count in both method and field domains. It observes zero
conversions at 448, exactly two at 449, and no further conversion at 450.
Sparse bucket capacity is zero after each conversion. Each one-member domain
stays sparse even after 100000 repeats.

The [census summary](evidence/hybrid-descriptors/v1/census-summary.json) retains
every domain and transition record. Partial overlap excludes the still-live
Cold temporary, fixed cache and allocator overhead. Instrumented conversion
duration excludes waiting, string construction, deque append and hash growth.
Neither is a normal trigger-API or whole-process measurement. See the complete
[source review](HYBRID-DESCRIPTORS-REVIEW.md) for all accounting limits.

## Validation and next bounded increment

198 native driver commands, 56 normal workload smoke runs and 11 QQ checks
passed their expected results. The required Core/JAR/JVM/AAR Gradle tasks
passed, with 71 JVM tests (zero skipped) and four Android ABI libraries. Host
ASan/UBSan ran with leak detection disabled. Android runtime/performance was
not measured. The five ABI normal/diagnostic layout probes matched. Full source
review covered the fixed increment and found no concrete publication/ownership
blocker; its reporting limits were adopted.

The remaining wide-lookup regression justified reading the existing generated
code. The old dense getter is frameless; the hybrid getter unconditionally
creates a 32-byte frame and stores the construction callback captures even for
a dense hit. This is an observed cost, not proof that it explains the entire
regression. A separate follow-up will put the existing fast-hit check before
callback materialization, preserve the current layout/threshold/publication,
and compare the changed entry against these fixed artifacts. This batch is
complete and will not be retuned.

Raw logs, ordered-byte fixture oracles, commands, manifests, source exports,
ABI probes and integrity hashes are in the
[evidence manifest](evidence/hybrid-descriptors/v1/manifest.json). APKs and
compiled artifacts are omitted; their identities are retained.
