# Uncached member descriptors: completed results

The native persistent method/field descriptor cache can be removed while
preserving the tested managed result contract. The opt-in implementation uses
owning result strings and raw input lookup. Performance has material tradeoffs;
keep UNCACHED_DESCRIPTORS OFF by default and retain the original dense backend.
The results below distinguish removal of persistent caching from switching the
lookup algorithm, and retain temporary output allocations and repeated work.
All comparisons below use the same other optimizations. Paired values are main /
confirmation; negative changes favor uncached. Intervals and limits are explained
under the fixed comparison.

## Findings that determine the decision

No tested complete-lifecycle comparison has a repeated speed benefit against
old dense. Against vector, repeated lifecycle benefits occur in wide lookup,
full method output once/twice, and changed-set field output. These are specific
advantages over the vector transition design, not a general speed advantage
over the original dense cache.

- QQ peak versus old dense changes -9.48% to -8.89% across the six observations; all peak intervals exclude zero.
  No QQ lifecycle comparison has repeated interval support. Against vector,
  both w4 peak comparisons remain unresolved; p1/w1 saves only about 0.3--0.4%.
  The low-output QQ case therefore gains little beyond the existing sparse
  vector configuration.

- Long output (4096 long methods, 16 calls) costs +101.28% [+89.09, +120.25] / +113.56% [+105.46, +117.31] versus old dense.
  Its peak rises +15.64% [+14.17, +15.82] / +15.64% [+15.56, +15.70]: 36.56 -> 42.29 / 36.56 -> 42.28 MiB.
  Against raw dense it remains +113.90% [+108.74, +118.18] / +112.77% [+103.34, +117.38] slower, so changing lookup cannot explain away this counterexample.
  Relative to vector its peak improves about 6.7--6.9%, but lifecycle still
  nearly doubles. Zero persistent cache does not guarantee a lower peak.

- Full field output sixteen times costs +53.12% [+24.07, +64.81] / +43.08% [+18.61, +55.57] versus old dense, while peak drops about 13.5--13.7%.
  Against raw dense lifecycle still costs +44.39% [+36.80, +48.66] / +46.65% [+33.41, +48.93]; repeated generation/result ownership is an independent tradeoff.
- The user's two-full-field case changes +13.79% [+4.65, +18.91] / +4.76% [-7.69, +18.99] versus old dense, and -11.29% [-36.55, +0.90] / -16.17% [-29.33, +5.41] versus vector.
  Neither timing difference has both intervals excluding zero. Its peak is
  clearly lower: about 13.5--13.6% versus old dense and 18.1--18.2% versus vector.
  Do not describe that noisy two-call timing as either proven equality or a
  confirmed removal of the earlier regression.

- Full method output once improves -28.55% / -23.21% versus vector; twice improves -21.73% / -25.59%. All four intervals are negative.
  The corresponding old-dense lifecycle differences remain unresolved in both
  phases; removing vector fill/transition overhead does not establish superiority
  to old dense.

- Wide lookup lifecycle improves -6.59% / -3.65% versus vector, but its repeated stage regresses +37.01% / +47.00%; all intervals exclude zero.
  Versus old dense, lifecycle regresses +21.22% / +27.31%, while peak changes 9.08 -> 1.81 / 9.08 -> 1.83 MiB.
  The lifecycle win against vector includes avoiding its initial sparse build,
  discard and regeneration; it is not a faster warmed lookup loop.

- Prefix lookup lifecycle regresses +167.59% [+162.81, +169.04] / +168.43% [+161.97, +175.63] versus old dense.
  Raw-dense/uncached changes only -0.40% [-0.70, +0.21] / +0.02% [-0.50, +0.43] and both intervals include zero.
  Most of that overall regression belongs to choosing raw input comparison,
  not a measured large incremental cache-removal penalty. Likewise, most of
  wide/prefix peak reduction is already present in raw dense: roughly 1.9/3.1
  MiB before uncached reaches 1.8/3.0 MiB. Attribute those savings accordingly.
- Narrow hot lookup retains a cache-removal/result-ownership penalty even with raw lookup held fixed: +7.57% [+5.21, +8.15] / +7.52% [+6.76, +7.88].
  Other small-output, low-unique and four-caller adverse cases stay in the full
  matrix below. Faster first stages or close do not erase their warm/lifecycle
  regressions.

## Fixed comparison and interpretation

All normal artifacts use `2be1a63f1ac9de56f6e5cd53123322388e598006` with the same Small14 combination and compiler.
The four variants differ only in VECTOR_DESCRIPTORS, UNCACHED_DESCRIPTORS and
RAW_DESCRIPTOR_LOOKUP. Control is the old dense backend within this optimization
combination, not unmodified upstream master.

| Variant | Persistent descriptor storage | Input descriptor lookup | Result descriptor |
| --- | --- | --- | --- |
| control | Old dense | Cached text | Borrowed cache view |
| raw-dense | Old dense | Raw component comparison | Borrowed cache view |
| vector | Sparse, then independent vector | Cached text | Borrowed generation view |
| uncached | None | Raw component comparison | Owning string |

Control/uncached and vector/uncached measure the overall proposal. Raw-dense/
uncached isolates cache removal plus owning-result costs under the same lookup
algorithm. It does not isolate each string copy, allocation or construction
individually. Do not subtract percentages from different paired sweeps as if
they were an additive causal decomposition.

The frozen harness/plan snapshot is `4462724487ef515eb682287564e50ab0c98d616b`.
Frozen plan SHA256: `c8326c5d06ba854fffd1b70663b595ce8b6e11f80cc245ac6f91beb9f09596e0`.
All 116 sweeps / 1392 fresh process samples completed, with no excluded successful
samples. Each sweep has six balanced AB/BA pairs, with a main phase and one
independently seeded confirmation. Frozen inputs reconciled before and after
timing. No builds, correctness checks, profiling or archive compression ran
during measurement. A separate four-process driver preflight is validation only.

Negative changes favor uncached. Values are main / confirmation paired median
percentage changes, not ratios of absolute medians. Bootstrap 95% intervals are
exploratory six-pair estimates without multiple-comparison adjustment. A result
is described as repeated support only when both intervals exclude zero in the
same direction. An interval crossing zero is not proof of equality.

Lifecycle includes create, setup, API stages, result destruction and close.
Four calling threads perform four times the work; they are separate workloads.
QQ workers are query-engine workers rather than simultaneous Java callers.
OS caches were not flushed. These are Apple M1 / macOS host measurements, not
Android device performance. Full absolute medians, MADs, intervals and samples
are retained in the [summary](evidence/uncached-descriptors/v1/summary.json).

## Decision cases

| Case / comparator | Lifecycle change and 95% intervals | Lifecycle support | Peak change and 95% intervals |
| --- | --- | --- | --- |
| wide / control | +21.22% [+18.29, +27.69] / +27.31% [+12.20, +32.80] | higher in both | -80.04% [-80.16, -79.85] / -79.85% [-80.01, -79.72] |
| wide / vector | -6.59% [-13.25, -3.90] / -3.65% [-4.51, -1.46] | lower in both | -89.05% [-89.15, -88.97] / -89.13% [-89.16, -89.05] |
| wide / raw-dense | +1.04% [-1.16, +2.52] / +0.70% [-0.11, +2.16] | not resolved in both | -3.70% [-4.52, -2.49] / -4.10% [-4.51, -3.30] |
| prefix / control | +167.59% [+162.81, +169.04] / +168.43% [+161.97, +175.63] | higher in both | -72.97% [-73.14, -72.72] / -73.02% [-73.19, -72.90] |
| prefix / vector | +151.28% [+146.44, +153.76] / +152.98% [+148.96, +155.28] | higher in both | -84.75% [-84.82, -84.68] / -84.70% [-84.76, -84.67] |
| prefix / raw-dense | -0.40% [-0.70, +0.21] / +0.02% [-0.50, +0.43] | not resolved in both | -2.78% [-3.53, -2.53] / -2.53% [-3.27, -1.53] |
| hot / control | +38.59% [+37.16, +39.79] / +38.81% [+38.55, +40.43] | higher in both | -8.47% [-8.47, -7.29] / -7.69% [-8.46, -7.62] |
| hot / vector | +34.25% [+33.88, +35.62] / +35.01% [+34.32, +35.59] | higher in both | -1.82% [-3.55, -1.82] / -1.80% [-4.00, -0.91] |
| hot / raw-dense | +7.57% [+5.21, +8.15] / +7.52% [+6.76, +7.88] | higher in both | -9.20% [-10.04, -8.39] / -9.24% [-10.41, -8.43] |
| long-output / control | +101.28% [+89.09, +120.25] / +113.56% [+105.46, +117.31] | higher in both | +15.64% [+14.17, +15.82] / +15.64% [+15.56, +15.70] |
| long-output / vector | +97.06% [+87.67, +103.90] / +97.36% [+94.25, +99.96] | higher in both | -6.74% [-6.84, -6.69] / -6.86% [-6.99, -6.79] |
| long-output / raw-dense | +113.90% [+108.74, +118.18] / +112.77% [+103.34, +117.38] | higher in both | +15.31% [+15.25, +15.41] / +15.32% [+15.20, +17.11] |
| method-full-1 / control | +8.77% [-17.73, +35.68] / -11.09% [-24.43, +16.50] | not resolved in both | -13.80% [-13.94, -13.78] / -13.82% [-14.00, -13.73] |
| method-full-1 / vector | -28.55% [-31.79, -19.44] / -23.21% [-30.83, -1.47] | lower in both | -15.37% [-15.51, -15.31] / -15.43% [-15.51, -15.35] |
| method-full-1 / raw-dense | +3.43% [-22.26, +14.76] / -4.90% [-8.49, -0.07] | not resolved in both | -13.94% [-14.01, -13.80] / -13.93% [-13.97, -13.92] |
| method-full-2 / control | +4.77% [+0.19, +20.40] / +2.66% [-11.28, +18.13] | not resolved in both | -13.70% [-13.91, -13.60] / -13.68% [-13.87, -13.65] |
| method-full-2 / vector | -21.73% [-26.54, -20.10] / -25.59% [-35.26, -14.35] | lower in both | -16.16% [-16.22, -16.14] / -16.20% [-17.61, -16.12] |
| method-full-2 / raw-dense | +18.04% [+3.78, +20.65] / +16.41% [+6.58, +19.98] | higher in both | -13.88% [-14.14, -13.78] / -13.81% [-13.87, -13.72] |
| field-full-1 / control | -17.46% [-30.32, +18.51] / -6.22% [-23.20, +18.04] | not resolved in both | -13.70% [-13.76, -13.65] / -13.73% [-13.80, -13.62] |
| field-full-1 / vector | -6.67% [-20.83, +2.24] / -9.85% [-25.67, +10.28] | not resolved in both | -18.16% [-18.31, -18.05] / -18.30% [-18.50, -18.26] |
| field-full-1 / raw-dense | -5.43% [-18.64, +16.82] / -15.91% [-27.32, -0.19] | not resolved in both | -13.70% [-13.77, -13.61] / -13.78% [-13.83, -13.63] |
| field-full-2 / control | +13.79% [+4.65, +18.91] / +4.76% [-7.69, +18.99] | not resolved in both | -13.63% [-13.75, -13.42] / -13.50% [-13.69, -13.45] |
| field-full-2 / vector | -11.29% [-36.55, +0.90] / -16.17% [-29.33, +5.41] | not resolved in both | -18.07% [-18.40, -18.04] / -18.19% [-18.28, -18.16] |
| field-full-2 / raw-dense | -2.19% [-18.69, +15.20] / -2.08% [-15.12, +20.07] | not resolved in both | -13.71% [-13.78, -13.61] / -13.69% [-13.76, -13.61] |
| field-full-16 / control | +53.12% [+24.07, +64.81] / +43.08% [+18.61, +55.57] | higher in both | -13.68% [-14.09, -10.90] / -13.53% [-13.56, -13.39] |
| field-full-16 / vector | +45.60% [+23.35, +63.77] / +40.37% [+29.10, +65.98] | higher in both | -18.05% [-19.00, -13.20] / -17.89% [-18.06, -17.65] |
| field-full-16 / raw-dense | +44.39% [+36.80, +48.66] / +46.65% [+33.41, +48.93] | higher in both | -13.56% [-14.05, -13.45] / -13.58% [-13.73, -13.39] |
| low-id-w1 / control | +25.52% [+13.85, +28.57] / +28.37% [+23.31, +39.26] | higher in both | -29.37% [-29.53, -29.28] / -29.49% [-29.62, -29.27] |
| low-id-w1 / vector | +31.54% [+6.37, +36.16] / +15.68% [+2.26, +32.59] | higher in both | -1.70% [-1.95, -1.53] / -1.95% [-2.45, -1.19] |
| field-full-w4 / control | +32.36% [+2.85, +43.69] / +20.15% [+3.22, +26.92] | higher in both | -3.63% [-6.34, -0.71] / +1.43% [-7.30, +4.19] |
| field-full-w4 / vector | +20.67% [+5.98, +31.26] / +22.75% [+9.75, +31.10] | higher in both | -8.12% [-17.51, -1.73] / -5.16% [-8.56, +9.70] |

## QQ workflows

| Comparison | Lifecycle change and 95% intervals | Peak change and 95% intervals | Absolute peak MiB, comparator -> uncached |
| --- | --- | --- | --- |
| control-to-uncached-qq-p1-w4 | -1.35% [-3.61, +1.65] / -3.56% [-14.05, -2.29] | -9.22% [-9.63, -8.95] / -9.48% [-9.74, -8.46] | 1478.29 -> 1340.53 / 1478.13 -> 1341.54 |
| vector-to-uncached-qq-p1-w4 | +0.26% [-0.56, +4.67] / +1.12% [-3.70, +6.63] | -0.17% [-0.62, +0.59] / -0.20% [-0.66, +0.60] | 1344.15 -> 1342.69 / 1344.00 -> 1340.78 |
| raw-dense-to-uncached-qq-p1-w4 | -4.21% [-9.22, +20.63] / -2.86% [-4.61, -0.84] | -9.26% [-9.76, -8.50] / -9.35% [-9.91, -8.96] | 1478.22 -> 1340.82 / 1478.84 -> 1340.40 |
| control-to-uncached-qq-p11-w4 | -1.26% [-3.08, +4.17] / -0.76% [-0.95, -0.39] | -9.34% [-9.53, -8.68] / -8.89% [-9.21, -8.63] | 1508.40 -> 1367.48 / 1503.43 -> 1369.75 |
| vector-to-uncached-qq-p11-w4 | +0.41% [-0.49, +3.62] / -0.08% [-1.45, +1.33] | +0.02% [-0.69, +0.77] / -0.46% [-0.96, +0.40] | 1373.61 -> 1377.65 / 1377.40 -> 1372.60 |
| raw-dense-to-uncached-qq-p11-w4 | -0.26% [-2.93, +0.79] / -0.98% [-3.22, +0.11] | -9.13% [-9.55, -8.30] / -9.29% [-9.60, -8.68] | 1507.26 -> 1370.79 / 1506.34 -> 1368.27 |
| control-to-uncached-qq-p1-w1 | -0.68% [-1.89, -0.35] / -2.23% [-3.43, +0.33] | -9.29% [-9.43, -8.81] / -9.33% [-9.53, -8.82] | 1472.95 -> 1336.85 / 1474.11 -> 1336.89 |
| vector-to-uncached-qq-p1-w1 | -0.09% [-2.50, +3.54] / -0.28% [-1.91, +6.23] | -0.31% [-0.48, -0.14] / -0.37% [-1.00, -0.30] | 1338.73 -> 1333.51 / 1342.39 -> 1335.75 |

## Complete native matrix

First/repeated/close columns show paired median changes; their complete intervals
are in the linked summary. A single full call has no repeated stage.

### control to uncached

| Workload | Lifecycle and 95% intervals | First stage | Repeated stage | Close | Peak and 95% intervals |
| --- | --- | --- | --- | --- | --- |
| wide | +21.22% [+18.29, +27.69] / +27.31% [+12.20, +32.80] | -98.65% / -98.62% | +54.60% / +62.24% | -63.79% / -65.72% | -80.04% [-80.16, -79.85] / -79.85% [-80.01, -79.72] |
| prefix | +167.59% [+162.81, +169.04] / +168.43% [+161.97, +175.63] | -72.69% / -71.26% | +179.72% / +180.35% | -68.17% / -68.52% | -72.97% [-73.14, -72.72] / -73.02% [-73.19, -72.90] |
| hot | +38.59% [+37.16, +39.79] / +38.81% [+38.55, +40.43] | -53.10% / -56.84% | +41.77% / +42.11% | -10.74% / -14.72% | -8.47% [-8.47, -7.29] / -7.69% [-8.46, -7.62] |
| long-output | +101.28% [+89.09, +120.25] / +113.56% [+105.46, +117.31] | +6.32% / +21.70% | +123.30% / +138.70% | -83.40% / -82.80% | +15.64% [+14.17, +15.82] / +15.64% [+15.56, +15.70] |
| sso-prefix | +3.52% [+1.59, +5.50] / +3.86% [+1.89, +4.90] | -8.64% / -6.15% | +39.42% / +39.11% | -66.94% / -69.08% | -23.56% [-23.93, -23.42] / -23.47% [-23.79, -23.00] |
| sso-scattered | +0.46% [-0.56, +1.20] / +3.29% [+1.10, +7.01] | -31.84% / -14.01% | +34.65% / +37.54% | -71.76% / -58.96% | -23.71% [-24.16, -23.38] / -23.69% [-24.16, -23.33] |
| narrow-w1 | +61.48% [+34.51, +68.00] / +63.46% [+56.38, +68.17] | -16.89% / -0.23% | +64.10% / +67.15% | +2.88% / +17.20% | -10.26% [-11.45, -9.33] / -8.92% [-10.22, -6.78] |
| narrow-w4 | +7.13% [-3.23, +24.06] / +10.51% [+3.70, +37.24] | -25.42% / -10.69% | +7.26% / +10.55% | -18.81% / -4.80% | -8.47% [-9.01, -6.46] / -7.82% [-8.97, -6.05] |
| method-full-1 | +8.77% [-17.73, +35.68] / -11.09% [-24.43, +16.50] | +9.94% / -5.79% | -- | -67.00% / -60.08% | -13.80% [-13.94, -13.78] / -13.82% [-14.00, -13.73] |
| method-full-2 | +4.77% [+0.19, +20.40] / +2.66% [-11.28, +18.13] | -0.94% / -3.40% | +41.38% / +26.76% | -23.11% / -55.39% | -13.70% [-13.91, -13.60] / -13.68% [-13.87, -13.65] |
| method-full-16 | +8.06% [-3.57, +28.45] / +4.38% [-1.34, +36.16] | -3.15% / -6.04% | +9.76% / +6.76% | -69.52% / -61.05% | -13.56% [-13.84, -13.28] / -13.63% [-13.82, -13.60] |
| field-full-1 | -17.46% [-30.32, +18.51] / -6.22% [-23.20, +18.04] | -18.94% / +0.27% | -- | -65.88% / -74.36% | -13.70% [-13.76, -13.65] / -13.73% [-13.80, -13.62] |
| field-full-2 | +13.79% [+4.65, +18.91] / +4.76% [-7.69, +18.99] | +0.23% / -3.28% | +57.26% / +31.34% | -61.06% / -58.19% | -13.63% [-13.75, -13.42] / -13.50% [-13.69, -13.45] |
| field-full-16 | +53.12% [+24.07, +64.81] / +43.08% [+18.61, +55.57] | -1.56% / +0.52% | +66.36% / +52.79% | -76.66% / -72.28% | -13.68% [-14.09, -10.90] / -13.53% [-13.56, -13.39] |
| method-changed-2 | -15.33% [-28.79, +12.86] / -22.39% [-30.60, -12.51] | -12.99% / -36.07% | -29.28% / -25.24% | -71.30% / -72.68% | -19.69% [-19.85, -19.46] / -19.56% [-19.60, -19.46] |
| low-id-w1 | +25.52% [+13.85, +28.57] / +28.37% [+23.31, +39.26] | -30.69% / +6.55% | +56.37% / +58.91% | -62.69% / -67.24% | -29.37% [-29.53, -29.28] / -29.49% [-29.62, -29.27] |
| low-id-w4 | +20.49% [-3.95, +32.39] / +32.18% [+9.84, +59.50] | +1.81% / +15.22% | +39.83% / +55.42% | -76.52% / -67.66% | -28.67% [-29.87, -28.30] / -28.48% [-29.17, -27.99] |
| method-full-w4 | +6.29% [-2.90, +24.04] / +10.15% [+1.58, +28.46] | -33.09% / -27.78% | +12.47% / +18.79% | -68.77% / -68.94% | -8.99% [-10.73, -7.54] / -7.03% [-10.89, -2.58] |
| field-changed-2 | -1.81% [-7.07, +3.58] / -5.88% [-9.69, +0.53] | -1.43% / -1.97% | +12.86% / -3.06% | -70.01% / -70.85% | -16.83% [-16.95, -16.52] / -16.60% [-18.91, -16.45] |
| field-full-w4 | +32.36% [+2.85, +43.69] / +20.15% [+3.22, +26.92] | -29.31% / -25.95% | +45.71% / +28.91% | -61.91% / -59.08% | -3.63% [-6.34, -0.71] / +1.43% [-7.30, +4.19] |

### vector to uncached

| Workload | Lifecycle and 95% intervals | First stage | Repeated stage | Close | Peak and 95% intervals |
| --- | --- | --- | --- | --- | --- |
| wide | -6.59% [-13.25, -3.90] / -3.65% [-4.51, -1.46] | -99.39% / -99.37% | +37.01% / +47.00% | -63.25% / -66.57% | -89.05% [-89.15, -88.97] / -89.13% [-89.16, -89.05] |
| prefix | +151.28% [+146.44, +153.76] / +152.98% [+148.96, +155.28] | -85.68% / -85.42% | +174.18% / +176.79% | -95.91% / -95.47% | -84.75% [-84.82, -84.68] / -84.70% [-84.76, -84.67] |
| hot | +34.25% [+33.88, +35.62] / +35.01% [+34.32, +35.59] | -71.30% / -72.38% | +36.84% / +37.84% | -12.92% / -4.08% | -1.82% [-3.55, -1.82] / -1.80% [-4.00, -0.91] |
| long-output | +97.06% [+87.67, +103.90] / +97.36% [+94.25, +99.96] | +10.49% / +11.48% | +119.84% / +119.39% | -79.68% / -80.89% | -6.74% [-6.84, -6.69] / -6.86% [-6.99, -6.79] |
| sso-prefix | +4.94% [+2.48, +10.84] / +5.20% [+4.41, +9.04] | -38.25% / -35.39% | +21.78% / +21.79% | -18.56% / -22.28% | -3.33% [-3.40, -3.14] / -3.59% [-3.72, -3.46] |
| sso-scattered | +6.61% [+2.93, +9.08] / +4.89% [+2.83, +6.73] | -29.34% / -26.57% | +25.54% / +21.60% | -13.90% / -42.27% | -3.58% [-4.09, -2.95] / -3.78% [-3.85, -3.14] |
| narrow-w1 | +2.62% [-5.49, +59.61] / -0.57% [-5.40, +5.63] | -46.35% / -30.26% | +2.72% / +0.42% | -19.94% / +5.90% | -3.84% [-4.76, -1.91] / -3.36% [-4.30, -2.40] |
| narrow-w4 | +2.21% [-11.94, +18.46] / -12.12% [-25.76, -11.54] | -31.60% / -58.76% | +2.27% / -12.10% | -0.82% / -6.54% | -2.74% [-4.97, -1.84] / -1.39% [-3.66, -0.46] |
| method-full-1 | -28.55% [-31.79, -19.44] / -23.21% [-30.83, -1.47] | -37.24% / -30.23% | -- | -48.01% / -59.10% | -15.37% [-15.51, -15.31] / -15.43% [-15.51, -15.35] |
| method-full-2 | -21.73% [-26.54, -20.10] / -25.59% [-35.26, -14.35] | -37.63% / -31.87% | -8.18% / -26.35% | -21.80% / -46.76% | -16.16% [-16.22, -16.14] / -16.20% [-17.61, -16.12] |
| method-full-16 | -4.59% [-7.94, +3.76] / -1.71% [-11.98, +0.22] | -36.09% / -36.14% | +0.71% / +3.33% | -39.71% / -51.60% | -16.15% [-17.50, -15.98] / -16.10% [-16.38, -15.93] |
| field-full-1 | -6.67% [-20.83, +2.24] / -9.85% [-25.67, +10.28] | -7.48% / -20.82% | -- | -56.13% / -68.43% | -18.16% [-18.31, -18.05] / -18.30% [-18.50, -18.26] |
| field-full-2 | -11.29% [-36.55, +0.90] / -16.17% [-29.33, +5.41] | -10.00% / -17.96% | -20.63% / -26.81% | -39.04% / -26.42% | -18.07% [-18.40, -18.04] / -18.19% [-18.28, -18.16] |
| field-full-16 | +45.60% [+23.35, +63.77] / +40.37% [+29.10, +65.98] | -5.97% / -15.41% | +59.14% / +68.27% | -28.14% / -29.97% | -18.05% [-19.00, -13.20] / -17.89% [-18.06, -17.65] |
| method-changed-2 | +2.38% [-22.28, +7.97] / -26.29% [-27.55, -21.18] | -2.30% / -35.54% | -0.19% / -36.72% | -29.57% / -27.45% | -22.41% [-22.45, -22.27] / -22.26% [-22.37, -22.04] |
| low-id-w1 | +31.54% [+6.37, +36.16] / +15.68% [+2.26, +32.59] | -33.33% / -22.13% | +44.76% / +26.10% | -15.21% / -13.44% | -1.70% [-1.95, -1.53] / -1.95% [-2.45, -1.19] |
| low-id-w4 | -1.64% [-6.84, +14.69] / +21.98% [-19.04, +27.50] | -26.37% / -26.85% | -2.43% / +30.75% | +3.40% / +18.92% | -0.25% [-1.17, -0.08] / -0.42% [-1.33, -0.08] |
| method-full-w4 | +0.92% [-5.86, +10.06] / -5.03% [-15.92, +3.92] | -59.23% / -45.19% | +10.66% / -1.17% | -52.21% / -25.42% | -14.05% [-18.69, -9.02] / -11.86% [-14.65, -10.83] |
| field-changed-2 | -21.06% [-26.99, -14.31] / -18.59% [-26.78, -11.08] | -39.17% / -33.73% | -2.59% / -7.77% | -33.95% / -20.73% | -14.14% [-14.47, -13.98] / -14.12% [-14.41, -13.91] |
| field-full-w4 | +20.67% [+5.98, +31.26] / +22.75% [+9.75, +31.10] | -45.44% / -51.50% | +30.45% / +44.57% | -25.40% / -29.11% | -8.12% [-17.51, -1.73] / -5.16% [-8.56, +9.70] |

### raw-dense to uncached

| Workload | Lifecycle and 95% intervals | First stage | Repeated stage | Close | Peak and 95% intervals |
| --- | --- | --- | --- | --- | --- |
| wide | +1.04% [-1.16, +2.52] / +0.70% [-0.11, +2.16] | +6.96% / +8.29% | +1.31% / +1.61% | -18.61% / -29.80% | -3.70% [-4.52, -2.49] / -4.10% [-4.51, -3.30] |
| prefix | -0.40% [-0.70, +0.21] / +0.02% [-0.50, +0.43] | -0.44% / -0.25% | -0.25% / +0.03% | -22.30% / +1.85% | -2.78% [-3.53, -2.53] / -2.53% [-3.27, -1.53] |
| hot | +7.57% [+5.21, +8.15] / +7.52% [+6.76, +7.88] | -6.34% / +4.92% | +8.10% / +7.95% | -10.24% / -4.57% | -9.20% [-10.04, -8.39] / -9.24% [-10.41, -8.43] |
| long-output | +113.90% [+108.74, +118.18] / +112.77% [+103.34, +117.38] | +16.03% / +10.11% | +140.88% / +142.14% | -85.56% / -81.86% | +15.31% [+15.25, +15.41] / +15.32% [+15.20, +17.11] |
| method-full-1 | +3.43% [-22.26, +14.76] / -4.90% [-8.49, -0.07] | +7.56% / -0.95% | -- | -62.35% / -66.24% | -13.94% [-14.01, -13.80] / -13.93% [-13.97, -13.92] |
| method-full-2 | +18.04% [+3.78, +20.65] / +16.41% [+6.58, +19.98] | +3.15% / +2.60% | +59.12% / +59.01% | -61.17% / -62.23% | -13.88% [-14.14, -13.78] / -13.81% [-13.87, -13.72] |
| method-full-16 | +4.80% [+1.59, +27.85] / +12.32% [+4.67, +32.15] | -8.33% / +3.13% | +7.81% / +15.50% | -59.65% / -65.84% | -13.69% [-15.29, -10.40] / -13.78% [-14.09, -13.33] |
| field-full-1 | -5.43% [-18.64, +16.82] / -15.91% [-27.32, -0.19] | -1.53% / -8.47% | -- | -75.30% / -72.37% | -13.70% [-13.77, -13.61] / -13.78% [-13.83, -13.63] |
| field-full-2 | -2.19% [-18.69, +15.20] / -2.08% [-15.12, +20.07] | -14.26% / -3.86% | +29.40% / +4.44% | -59.49% / -60.43% | -13.71% [-13.78, -13.61] / -13.69% [-13.76, -13.61] |
| field-full-16 | +44.39% [+36.80, +48.66] / +46.65% [+33.41, +48.93] | +0.50% / -5.56% | +52.74% / +60.01% | -77.26% / -72.78% | -13.56% [-14.05, -13.45] / -13.58% [-13.73, -13.39] |

## Physical peak in representative workloads

| Case / comparator | Peak MiB, comparator -> uncached, main / confirmation | Peak support |
| --- | --- | --- |
| wide / control | 9.08 -> 1.81 / 9.08 -> 1.83 | lower in both |
| wide / vector | 16.71 -> 1.83 / 16.70 -> 1.81 | lower in both |
| prefix / control | 11.17 -> 3.02 / 11.18 -> 3.02 | lower in both |
| prefix / vector | 19.69 -> 3.00 / 19.69 -> 3.02 | lower in both |
| hot / control | 1.84 -> 1.69 / 1.84 -> 1.69 | lower in both |
| hot / vector | 1.72 -> 1.69 / 1.74 -> 1.70 | lower in both |
| long-output / control | 36.56 -> 42.29 / 36.56 -> 42.28 | higher in both |
| long-output / vector | 45.34 -> 42.27 / 45.44 -> 42.29 | lower in both |
| method-full-1 / control | 31.92 -> 27.52 / 31.93 -> 27.52 | lower in both |
| method-full-1 / vector | 32.52 -> 27.52 / 32.52 -> 27.52 | lower in both |
| method-full-2 / control | 31.99 -> 27.62 / 32.00 -> 27.60 | lower in both |
| method-full-2 / vector | 32.95 -> 27.62 / 32.95 -> 27.61 | lower in both |
| method-full-16 / control | 32.17 -> 27.80 / 32.17 -> 27.78 | lower in both |
| method-full-16 / vector | 33.13 -> 27.81 / 33.09 -> 27.77 | lower in both |
| field-full-1 / control | 29.19 -> 25.20 / 29.20 -> 25.19 | lower in both |
| field-full-1 / vector | 30.79 -> 25.20 / 30.80 -> 25.16 | lower in both |
| field-full-2 / control | 29.26 -> 25.28 / 29.24 -> 25.29 | lower in both |
| field-full-2 / vector | 30.88 -> 25.29 / 30.89 -> 25.27 | lower in both |
| field-full-16 / control | 29.46 -> 25.43 / 29.41 -> 25.44 | lower in both |
| field-full-16 / vector | 31.07 -> 25.45 / 31.06 -> 25.49 | lower in both |
| method-full-w4 / control | 90.07 -> 81.65 / 90.54 -> 85.24 | lower in both |
| method-full-w4 / vector | 92.33 -> 81.84 / 91.70 -> 81.13 | lower in both |
| field-full-w4 / control | 78.74 -> 75.79 / 77.27 -> 76.27 | not resolved in both |
| field-full-w4 / vector | 80.55 -> 75.52 / 79.06 -> 74.85 | not resolved in both |

## Mechanism and accounting boundaries

Uncached removes the persistent method/field descriptor owners, arrays, ready
bytes and descriptor publication locks. It does not use vector generation
maintenance or borrowing scopes. Existing query admission, raw identity
publication and bridge lifecycle restrictions remain in effect.

Sixty native diagnostic cases retain access/build counts. Uncached
has zero persistent descriptor census and zero getter cache hits. Full field
output twice builds 120000 descriptors; sixteen passes build 960000. Long output
builds 65536 method descriptors. Wide input lookup builds 513 descriptors: one
during setup and one for each of 512 hits, while raw mismatching candidates do
not require descriptor construction. Counted builds are not total allocations:
Bean copies, FlatBuffer copies and allocator costs are not counted by Built.

Find collects owning Beans before creating text-deduplication views, and does
not move them during deduplication. Batch and existing result assembly retain
their actual copying behavior; this candidate does not add unrelated move
optimizations. Output lifetimes and duplicate representative selection remain
covered by the independent result oracles.

A host checker linked the unchanged normal Core observes the 14-byte method and
13-byte field inline, each with capacity 22. The 1778-byte method owns heap
capacity 2815, while its copied Bean has capacity 1783. The 30-byte field has
capacity 47 before copying and 31 afterward. These are concrete requested
capacities, not allocator size classes or process footprint. They explain why
equal bytes do not imply equal allocation behavior, without assigning all peak
changes to one cause.

| ABI | MethodBean, borrowed -> owning | FieldBean, borrowed -> owning |
| --- | --- | --- |
| desktop | 64 -> 72 | 40 -> 48 |
| arm64-v8a | 64 -> 72 | 40 -> 48 |
| armeabi-v7a | 40 -> 44 | 28 -> 32 |
| x86 | 40 -> 44 | 28 -> 32 |
| x86_64 | 64 -> 72 | 40 -> 48 |

Bean growth is temporary result storage, separate from the persistent-cache
census. Android rows are cross-compiled layout evidence only. Physical process
peak can include overlapping result owners, construction slack, copies, raw
input and allocator retention even though persistent descriptor cache is zero.

## Correctness and source review

- 193 native driver commands passed: normal 47, diagnostic 80, minimal 19,
  ASan/UBSan 24 and all-experiments-OFF 23. They preserve symbol/overload bytes,
  cross-DEX ordering/representatives, malformed lookups, relations, field and
  invocation oracles, workers and concurrent public API checks.
- Owning-value checks cover copy/move/assignment, vector relocation, independent
  source mutation, worker-returned Beans and serialized member results after
  bridge destruction. An additional normal-host checker observes actual inline
  and heap shape while repeating those ownership checks against frozen Core.
- All 80 normal smoke cases agreed; transition counts and ID sums also match
  independent calculations. Sixty native diagnostic processes reconcile caching
  and repeated-construction behavior separately from normal timing.
- Thirteen complete QQ oracle runs passed: eight normal, three diagnostic, and
  two final field-relation runs. Verification includes descriptors, selected
  results, order where relevant and unchanged empty/fallback outcomes.
- Required Core/JAR/JVM/Android tasks passed: 71 JVM tests, zero skipped, and an
  AAR containing arm64-v8a, armeabi-v7a, x86 and x86_64. Five ABI Bean layout
  probes and twelve CMake dependency/exclusion configurations passed.
- Documentation dependency install and VuePress build passed. Sanitizer leak
  detection was disabled; no Android device execution/performance is claimed.
- The complete 8m45s [source review](UNCACHED-DESCRIPTORS-REVIEW.md) read the fixed
  production increment and found no confirmed blocker. Review does not replace
  the executed verification or performance measurements.

The experiment changes native C++ types/ABI: MethodBean/FieldBean descriptors
own strings, and a view derived from a temporary or invalidated owning value
can dangle. Other native raw/query views, including complete AnnotationBean and
Batch wrappers, retain their old lifetime requirements. Java/Kotlin result
schema and behavior remain unchanged.

## Reproduction and retained evidence

The [manifest](evidence/uncached-descriptors/v1/manifest.json) records the archive
hash and readback verification. The [source identity](evidence/uncached-descriptors/v1/source-identity.json)
separates frozen engine and harness, and the [validation summary](evidence/uncached-descriptors/v1/validation-summary.json)
links all checks. The archive retains raw successful samples, separate development/
preflight records, verification outputs, fixture manifests, copied relevant
source, compile/configuration records and experiment drivers. APK/DEX/corpus
and compiled native/JVM/Android binaries are omitted; hashes identify separately
obtained inputs and artifacts. No production tuning or extra timing was performed
after the fixed batch.
