# Compact caller results

The counted, exact-allocation caller index is a useful memory and cold-lifecycle
optimization on this QQ workload. Across two independent runs, four-worker QQ
create-to-close time improves by 13.88-16.02% for one pass and 4.90-5.07% for
11 passes. One-worker, one-pass time improves by 10.68-11.98%. Caller directory
and edge capacity falls from 160.035 MiB to 103.275 MiB, saving 56.760 MiB
(35.47%). Measured QQ physical footprint peaks fall by about 54-68 MiB.

Keep COMPACT_CALLERS as a default-OFF experiment. The results support retaining
it as a candidate addition to the Small profile, with explicit tradeoffs:
repeated QQ queries are statistically unresolved, several exhaustive caller
traversals are around 1-2% slower, and tiny late construction adds about 0.06 ms.
No public API/schema changes or default enablement are part of this increment.

## Fixed comparison and method

Engine: `e3ec3622ff9bc3b3613f934356bbd1cf6b4408c5`. Both binaries use the same
source and compiler settings and differ only in COMPACT_CALLERS. Control is the
13-option Small profile after the ordinary-find extraction: LAZY_DIRECTORIES,
COMPACT_STRINGS, NEGATIVE_STRINGS, STRUCTURAL_DESCRIPTORS,
DESCRIPTOR_FAST_HITS, RAW_INTERFACES, FIELD_IDENTITY_SPLIT, COMPACT_INVOKES,
SINGLE_RELATION, INVERTED_STRINGS, INVERTED_STRING_RANGES,
SKIP_EMPTY_CANDIDATES and MOVE_FIND_RESULTS. CANDIDATE_PIPELINE and
CANDIDATE_SLICES are OFF. Compact adds only COMPACT_CALLERS.

Host: Apple M1, macOS 26.5.1. Native builds use Apple Clang 21, arm64,
`-O3 -DNDEBUG`, C++20 and a macOS 11.0 deployment target. Metrics and diagnostics
are compiled out for timing. QQ uses JBR 17.0.6, a fixed 256 MiB JVM heap and
AlwaysPreTouch. Artifact, adapter, fixture, JAR, JDK and probe hashes are recorded.

The exact 17-case list was frozen before timing and repeated in an independent
confirmation: 34 sweeps, six balanced randomized AB/BA pairs per sweep, 408
fresh processes. All successful samples are retained, including outliers; none
were excluded or replaced. Builds and functional checks did not overlap timing.
The QQ launcher compiles its adapter serially before each JVM, outside the
measured create-to-close interval. OS file caches were not flushed. This is a
host measurement, with Android build coverage rather than Android device timing.

Every arrow below is control median -> compact median. Percentages are medians
of paired relative changes, which can differ from ratios of the two medians.
Brackets are exploratory 95% paired-median bootstrap intervals (4,000 draws,
six pairs); they do not establish a universal regression bound. Negative is
better for time and memory. Complete samples and every staged metric are in the
evidence archive.

## QQ time

Lifecycle is the observed bridge create-to-close interval, including query
output handling and close. It excludes JVM startup and adapter compilation.
Each `all` pass follows the frozen QAuxiliary query flow on QQ 9.3.55.

| Run | Lifecycle ms | Paired change [95% interval] |
| --- | --- | --- |
| main-qq-p1-w4 | 1436.702 -> 1197.453 | -16.02% [-18.82, -10.27] |
| main-qq-p11-w4 | 4339.804 -> 4129.362 | -5.07% [-5.95, -4.43] |
| main-qq-p1-w1 | 3039.255 -> 2745.001 | -10.68% [-12.96, -5.97] |
| confirm-qq-p1-w4 | 1378.515 -> 1187.112 | -13.88% [-15.82, -9.99] |
| confirm-qq-p11-w4 | 4363.460 -> 4186.149 | -4.90% [-7.99, -0.19] |
| confirm-qq-p1-w1 | 2993.909 -> 2632.667 | -11.98% [-13.67, -11.27] |

The first pass improves in every QQ sweep. Close improves by about 41-46%,
consistent with replacing roughly 1.32 million retained caller buffers with
82 directory/payload allocations. This is an interpretation of the allocation
change, not an isolated causal measurement. The ten repeated passes are neutral
within their intervals. Create does not have a consistent gain; the confirmation
11-pass case records +5.93% [0.98, 12.99] there even while lifecycle improves.

| Run | Create change | First API pass change | Passes 1-10 API change | Close change |
| --- | --- | --- | --- | --- |
| main-qq-p1-w4 | +2.94% [-3.07, +16.76] | -12.37% [-14.54, -7.88] | n/a | -45.55% [-48.12, -39.74] |
| main-qq-p11-w4 | +1.15% [-1.13, +5.07] | -11.31% [-12.67, -10.21] | -0.07% [-0.75, +0.88] | -42.76% [-46.44, -41.26] |
| main-qq-p1-w1 | +1.73% [-9.36, +4.85] | -7.56% [-11.39, -2.72] | n/a | -41.14% [-42.95, -38.88] |
| confirm-qq-p1-w4 | +1.29% [-3.19, +13.43] | -10.20% [-10.95, -4.95] | n/a | -42.47% [-44.46, -40.41] |
| confirm-qq-p11-w4 | +5.93% [+0.98, +12.99] | -10.08% [-11.77, -0.75] | -0.84% [-2.30, +4.68] | -45.88% [-55.89, -33.75] |
| confirm-qq-p1-w1 | -3.09% [-11.75, -1.17] | -9.63% [-10.52, -8.87] | n/a | -42.70% [-44.67, -41.19] |

## QQ physical memory

| Run | Peak footprint MiB | Footprint change | Max RSS MiB | RSS change |
| --- | --- | --- | --- | --- |
| main-qq-p1-w4 | 1539.755 -> 1480.528 | -3.68% [-4.41, -3.53] | 1464.648 -> 1500.523 | +0.67% [-15.44, +7.90] |
| main-qq-p11-w4 | 1565.740 -> 1504.708 | -3.58% [-4.04, -3.35] | 1599.047 -> 1544.328 | -3.61% [-5.14, -3.07] |
| main-qq-p1-w1 | 1530.068 -> 1472.763 | -3.93% [-4.24, -2.99] | 1497.102 -> 1419.055 | -2.86% [-7.95, +0.44] |
| confirm-qq-p1-w4 | 1534.029 -> 1476.357 | -3.84% [-3.99, -3.50] | 1560.898 -> 1511.148 | -3.45% [-6.52, -2.30] |
| confirm-qq-p11-w4 | 1566.904 -> 1512.903 | -3.45% [-4.36, -2.59] | 1594.758 -> 1530.188 | -4.17% [-11.45, -3.11] |
| confirm-qq-p1-w1 | 1535.755 -> 1468.161 | -4.44% [-5.04, -3.70] | 1572.328 -> 1504.281 | -4.39% [-4.82, -0.24] |

Physical footprint improves in all six sweeps. RSS is less consistent:
the main four-worker single-pass estimate is +0.67% with an interval crossing
zero. Neither RSS nor physical footprint is caller-exclusive heap usage; both
also include mapped DEX data, other caches, JVM state, allocator behavior and
harness allocations. Reported in-window peak metrics are retained in JSON too.

## Exact caller capacity and build storage

QQ contains 41 DEX files with 392.742 MiB of raw uncompressed DEX. The caller
table covers 2,600,031 method IDs and 10,936,362 ordered edges. There are
1,315,612 nonempty final rows. All 41 per-DEX ordered caller hashes, row counts,
edge counts and largest-row counts agree between diagnostic control and compact.
The largest final row has 534,428 entries.

| Retained category | Control MiB | Compact MiB | Saved MiB |
| --- | --- | --- | --- |
| Row directory | 59.510 | 19.837 | 39.673 |
| Edge capacity | 100.525 | 83.438 | 17.087 |
| Total | 160.035 | 103.275 | 56.760 |

The final compact edge capacity is exactly 10,936,362 eight-byte entries;
the old retained capacity is 13,175,981 entries. The compact directory has one
size_t offset per method plus one sentinel per DEX. Totals exclude the small
per-DEX owner objects and allocator metadata. Retained buffer counts are
1,315,653 -> 82 (41 row directories plus 41 nonempty edge arrays).

| Snapshot | Directory MiB | Edges MiB | Count/cursor MiB | Pending imports MiB | Tracked total MiB |
| --- | --- | --- | --- | --- | --- |
| control / before_aggregate | 59.510 | 114.397 | 0.000 | 6.633 | 180.540 |
| control / released | 59.510 | 100.525 | 0.000 | 0.000 | 160.035 |
| compact / counted | 0.000 | 0.000 | 19.837 | 13.266 | 33.102 |
| compact / allocated | 19.837 | 83.438 | 19.837 | 13.266 | 136.377 |
| compact / released | 19.837 | 83.438 | 0.000 | 0.000 | 103.275 |

The 19.837 MiB count array becomes the write cursor array and is then freed.
Pending imports also drop to zero capacity before publication. Their temporary
capacity grows from 6.633 MiB to 13.266 MiB because each pending record retains a
size_t source length; this is included above. At the allocated snapshot compact
has 33.102 MiB of temporary count/import capacity. `counted` final_edges=0 means
the final layout has not been allocated yet; it does not mean there were no calls.

These are capacity snapshots, not measured allocator peaks. In particular, old
aggregation can temporarily overlap allocations as rows grow, and allocator
rounding is outside these totals. Physical peaks must be assessed from the
ordinary-build measurements, rather than inferred from this table.

## Bounded native workloads

Tiny, mixed and giant use the existing 26-, 98,306- and 262,146-edge fixtures.
Mostly-empty uses nine DEX files, eight sources with 5,000 methods each, and
60,008 edges concentrated into shared targets while most reverse rows stay
empty. One-source has three DEX files and 131,074 edges, with one method making
131,072 calls. Each fresh process performs 16 API repetitions after preparation.

Cold preparation includes instruction extraction, forward invocation storage,
identity resolution and caller completion. Late first builds forward invocations,
then times caller completion separately. Full builds all caches. These nested
preparation intervals already belong to setup/lifecycle and must not be summed
again. API columns for these staged modes are warm even when the preparation
mode is named cold. Lifecycle also includes queries, output serialization,
destruction and the memory probes.

### Main construction and lifecycle

| Fixture / mode | Preparation ms | Preparation change | Lifecycle ms | Lifecycle change |
| --- | --- | --- | --- | --- |
| tiny / caller-match-cold-w1 | 0.059 -> 0.060 | -4.57% [-21.13, +30.49] | 0.893 -> 1.006 | +5.04% [-18.64, +27.51] |
| tiny / caller-output-late-w4 | 0.094 -> 0.158 | +65.07% [+23.13, +115.12] | 0.460 -> 0.553 | +29.02% [+5.75, +52.09] |
| mixed / caller-match-cold-w4 | 0.687 -> 0.629 | -8.36% [-20.15, +23.62] | 200.676 -> 202.845 | +1.03% [-1.83, +1.91] |
| mixed / caller-match-late-w4 | 0.607 -> 0.399 | -34.28% [-53.37, -25.63] | 201.330 -> 203.149 | +0.95% [+0.80, +1.20] |
| mixed / caller-multiple-late-w4 | 0.604 -> 0.390 | -35.65% [-39.33, -32.92] | 14.819 -> 14.380 | -2.41% [-5.94, -1.44] |
| mixed / caller-output-full-w4 | 0.709 -> 0.735 | +3.70% [-4.90, +11.26] | 126.167 -> 127.011 | +1.11% [-3.53, +2.85] |
| giant / caller-early-cold-w4 | 1.669 -> 1.331 | -19.34% [-24.80, -13.05] | 2.680 -> 2.227 | -16.26% [-25.96, -8.56] |
| giant / caller-match-cold-w4 | 1.601 -> 1.363 | -13.99% [-22.81, -7.20] | 532.825 -> 539.406 | +1.08% [+0.87, +1.53] |
| giant / caller-multiple-late-w1 | 2.337 -> 1.389 | -40.83% [-43.35, -36.47] | 38.777 -> 37.124 | -4.65% [-5.32, +8.52] |
| giant / caller-output-cold-w4 | 1.626 -> 1.376 | -16.07% [-18.51, -10.72] | 346.982 -> 347.473 | +0.28% [-0.30, +1.67] |
| mostly-empty / caller-match-cold-w1 | 1.153 -> 1.220 | +5.91% [-0.16, +19.37] | 132.635 -> 134.610 | +1.47% [+1.32, +1.67] |
| mostly-empty / caller-match-late-w4 | 0.569 -> 0.533 | -5.36% [-24.48, +24.55] | 110.084 -> 111.074 | +0.84% [+0.61, +1.22] |
| one-source / caller-match-cold-w1 | 1.313 -> 1.170 | -10.63% [-16.10, -5.47] | 269.241 -> 269.569 | +0.04% [-9.00, +1.24] |
| one-source / caller-match-cold-w4 | 1.433 -> 1.268 | -13.17% [-20.10, -2.37] | 272.448 -> 273.547 | +0.37% [-1.07, +1.27] |

### Confirm construction and lifecycle

| Fixture / mode | Preparation ms | Preparation change | Lifecycle ms | Lifecycle change |
| --- | --- | --- | --- | --- |
| tiny / caller-match-cold-w1 | 0.052 -> 0.049 | +4.55% [-29.41, +23.60] | 0.846 -> 0.771 | +0.38% [-19.01, +15.24] |
| tiny / caller-output-late-w4 | 0.092 -> 0.150 | +54.35% [+32.90, +94.38] | 0.440 -> 0.507 | +8.22% [+2.44, +33.19] |
| mixed / caller-match-cold-w4 | 0.690 -> 0.604 | -10.88% [-23.50, +0.71] | 198.787 -> 201.263 | +1.21% [+0.67, +1.67] |
| mixed / caller-match-late-w4 | 0.599 -> 0.424 | -31.32% [-42.62, -17.65] | 199.575 -> 201.498 | +0.97% [-0.12, +1.11] |
| mixed / caller-multiple-late-w4 | 0.606 -> 0.406 | -34.04% [-37.98, -29.46] | 14.447 -> 14.246 | -0.93% [-2.30, -0.54] |
| mixed / caller-output-full-w4 | 0.750 -> 0.684 | -9.30% [-12.88, -5.32] | 124.596 -> 124.787 | +0.48% [-0.38, +0.98] |
| giant / caller-early-cold-w4 | 1.568 -> 1.380 | -11.56% [-16.56, -3.30] | 2.484 -> 2.283 | -7.12% [-11.86, +0.41] |
| giant / caller-match-cold-w4 | 1.554 -> 1.364 | -13.76% [-17.46, -3.82] | 529.721 -> 534.279 | +0.85% [+0.45, +1.35] |
| giant / caller-multiple-late-w1 | 2.327 -> 1.338 | -43.22% [-44.35, -39.68] | 38.528 -> 36.551 | -4.61% [-5.43, -3.56] |
| giant / caller-output-cold-w4 | 1.594 -> 1.325 | -16.95% [-25.15, -5.33] | 336.922 -> 338.119 | +0.25% [-0.17, +5.73] |
| mostly-empty / caller-match-cold-w1 | 1.128 -> 1.189 | +5.36% [-2.16, +12.14] | 129.667 -> 131.386 | +1.55% [+0.90, +1.60] |
| mostly-empty / caller-match-late-w4 | 0.496 -> 0.563 | +14.32% [+7.06, +23.58] | 107.650 -> 109.442 | +1.77% [+1.24, +69.16] |
| one-source / caller-match-cold-w1 | 1.315 -> 1.166 | -11.55% [-13.86, -8.58] | 264.917 -> 268.159 | +1.12% [+0.50, +1.77] |
| one-source / caller-match-cold-w4 | 1.398 -> 1.323 | -7.90% [-13.51, +3.15] | 265.705 -> 268.018 | +0.85% [+0.60, +1.10] |

The giant early-hit lifecycle improves by 16.26% in main and 7.12% in
confirmation, although the latter interval crosses zero. Mixed late multiple
matching improves in both phases, and giant one-worker late multiple matching
has estimates near -4.6%. Output-heavy lifecycle changes remain unresolved.

Regressions are retained explicitly. Tiny late preparation goes from about
0.09 ms to 0.15-0.16 ms, with lifecycle +29.02% in main and +8.22% in confirmation
(absolute medians 0.460 -> 0.553 ms and 0.440 -> 0.507 ms). Exhaustive matching
on mixed/giant and mostly-empty rows is usually around 1-2% slower end to end.
One-source confirmation is +1.12% with one worker and +0.85% with four workers;
the main intervals crossed zero. Mostly-empty late construction changes sign:
-5.36% [-24.48, 24.55] in main, +14.32% [7.06, 23.58] in confirmation.

The mostly-empty late confirmation retains a large outlier: lifecycle median
change is +1.77%, but its bootstrap interval extends to +69.16%. This workload
cannot support a tight worst-case regression claim. No sample was removed to
narrow that interval.

### Warm APIs and memory

Warm allocator snapshots are after index preparation and before timed queries.
Closed snapshots are after bridge destruction while the small query/metadata
inputs still exist; their later destruction is inside lifecycle. JSON retains
both closed footprint and closed malloc values. The malloc column below is
whole-process allocator in-use bytes, not an isolated caller heap.

#### Main

| Fixture / mode | Repeated API ms (15) | API change | Warm malloc MiB | Peak footprint MiB | Peak change |
| --- | --- | --- | --- | --- | --- |
| tiny / caller-match-cold-w1 | 0.425 -> 0.473 | +8.44% [+1.34, +32.99] | 0.090 -> 0.090 | 1.446 -> 1.462 | +2.17% [-1.07, +4.97] |
| tiny / caller-output-late-w4 | 0.025 -> 0.025 | -3.92% [-6.06, +16.61] | 0.090 -> 0.090 | 1.266 -> 1.290 | +1.23% [-1.81, +2.47] |
| mixed / caller-match-cold-w4 | 186.997 -> 188.989 | +1.07% [-1.10, +1.89] | 1.355 -> 1.367 | 5.095 -> 3.868 | -24.15% [-27.02, -20.16] |
| mixed / caller-match-late-w4 | 186.936 -> 189.184 | +1.20% [+1.04, +1.41] | 1.355 -> 1.367 | 5.118 -> 3.759 | -25.29% [-29.94, -14.28] |
| mixed / caller-multiple-late-w4 | 12.406 -> 12.320 | -0.27% [-2.73, +0.85] | 1.355 -> 1.367 | 6.048 -> 4.337 | -27.26% [-38.34, -16.64] |
| mixed / caller-output-full-w4 | 114.786 -> 114.933 | +0.47% [-3.35, +1.41] | 1.566 -> 1.579 | 41.470 -> 41.923 | +0.62% [+0.02, +1.15] |
| giant / caller-early-cold-w4 | 0.219 -> 0.201 | -2.50% [-26.86, +22.31] | 4.088 -> 4.104 | 10.345 -> 8.118 | -21.57% [-22.93, -20.53] |
| giant / caller-match-cold-w4 | 497.451 -> 503.527 | +1.12% [+0.87, +1.59] | 4.088 -> 4.104 | 10.298 -> 8.056 | -21.77% [-24.98, -19.13] |
| giant / caller-multiple-late-w1 | 31.870 -> 31.278 | -1.91% [-2.70, +12.34] | 4.088 -> 4.104 | 12.290 -> 8.204 | -33.27% [-33.37, -33.05] |
| giant / caller-output-cold-w4 | 314.885 -> 315.081 | +0.01% [-0.72, +0.44] | 4.088 -> 4.104 | 122.220 -> 121.212 | -0.80% [-1.21, -0.72] |
| mostly-empty / caller-match-cold-w1 | 120.489 -> 122.404 | +1.64% [+1.30, +1.91] | 5.676 -> 5.051 | 10.298 -> 9.407 | -8.57% [-8.72, -7.89] |
| mostly-empty / caller-match-late-w4 | 99.598 -> 100.493 | +0.91% [+0.52, +1.29] | 5.676 -> 5.051 | 10.532 -> 9.642 | -8.46% [-9.09, -8.17] |
| one-source / caller-match-cold-w1 | 250.770 -> 251.109 | +0.05% [-9.03, +1.30] | 2.088 -> 2.104 | 5.798 -> 4.813 | -17.20% [-17.65, -16.48] |
| one-source / caller-match-cold-w4 | 253.585 -> 254.722 | +0.35% [-0.81, +1.39] | 2.088 -> 2.104 | 5.876 -> 4.876 | -17.13% [-17.26, -16.68] |

#### Confirm

| Fixture / mode | Repeated API ms (15) | API change | Warm malloc MiB | Peak footprint MiB | Peak change |
| --- | --- | --- | --- | --- | --- |
| tiny / caller-match-cold-w1 | 0.387 -> 0.374 | -3.15% [-14.93, +6.66] | 0.090 -> 0.090 | 1.454 -> 1.470 | +1.65% [+0.00, +3.81] |
| tiny / caller-output-late-w4 | 0.025 -> 0.024 | -4.92% [-9.57, +3.08] | 0.090 -> 0.090 | 1.266 -> 1.290 | +1.23% [-1.84, +2.47] |
| mixed / caller-match-cold-w4 | 185.244 -> 187.592 | +1.27% [+0.77, +1.63] | 1.355 -> 1.367 | 4.821 -> 3.751 | -21.95% [-27.61, -18.88] |
| mixed / caller-match-late-w4 | 185.709 -> 187.621 | +1.06% [-0.14, +1.17] | 1.355 -> 1.367 | 5.157 -> 3.876 | -25.07% [-28.57, -12.31] |
| mixed / caller-multiple-late-w4 | 12.012 -> 12.081 | +0.36% [-0.74, +1.64] | 1.355 -> 1.367 | 6.149 -> 4.290 | -30.14% [-31.25, -24.77] |
| mixed / caller-output-full-w4 | 113.013 -> 113.232 | +0.36% [-0.45, +0.90] | 1.566 -> 1.579 | 41.321 -> 41.954 | +1.40% [-0.92, +1.74] |
| giant / caller-early-cold-w4 | 0.204 -> 0.218 | +3.21% [-2.78, +20.79] | 4.088 -> 4.104 | 10.274 -> 8.157 | -20.53% [-21.25, -20.26] |
| giant / caller-match-cold-w4 | 493.555 -> 498.922 | +1.12% [+0.47, +1.46] | 4.088 -> 4.104 | 10.329 -> 8.165 | -20.72% [-21.69, -20.38] |
| giant / caller-multiple-late-w1 | 31.622 -> 30.840 | -2.48% [-2.77, -0.70] | 4.088 -> 4.104 | 12.267 -> 8.212 | -33.01% [-33.39, -31.94] |
| giant / caller-output-cold-w4 | 306.433 -> 307.600 | +0.28% [-0.33, +6.32] | 4.088 -> 4.104 | 122.165 -> 121.220 | -0.74% [-0.84, -0.15] |
| mostly-empty / caller-match-cold-w1 | 117.846 -> 119.413 | +1.55% [+0.84, +1.68] | 5.676 -> 5.051 | 10.282 -> 9.407 | -8.51% [-8.86, -7.66] |
| mostly-empty / caller-match-late-w4 | 97.392 -> 99.218 | +1.79% [+1.27, +63.86] | 5.676 -> 5.051 | 10.423 -> 9.610 | -7.79% [-8.56, -7.28] |
| one-source / caller-match-cold-w1 | 246.673 -> 249.847 | +1.22% [+0.54, +1.86] | 2.088 -> 2.104 | 5.806 -> 4.806 | -17.00% [-17.47, -16.69] |
| one-source / caller-match-cold-w4 | 247.318 -> 249.471 | +0.88% [+0.63, +1.12] | 2.088 -> 2.104 | 5.892 -> 4.876 | -17.11% [-17.72, -16.78] |

Warm malloc in-use capacity does not improve on every fixture: the dense
mixed/giant/one-source cases have small increases of about 13-16 KiB, while the
mostly-empty case drops by 0.625 MiB. Dense exact rows already offer little
payload slack to remove, and allocator rounding/owner overhead still matters.
Physical peaks improve for many long-row match cases; full output and tiny
cases do not show a universal peak reduction.

## Implementation and validation

Cold extraction counts while decoding existing invoke instructions. Late
construction counts the existing forward rows. Final caller storage consists
of an M+1 prefix directory and an exact uninitialized array of trivial 8-byte
entries. Existing cross-DEX identity binding is preserved; local contributions
precede source-DEX/work-list contributions, duplicates remain duplicated, and
transferred reference rows remain empty. Preassigned disjoint segments permit
parallel source filling without changing result order. The existing warmup
barrier publishes callers only after workers join and temporary capacity is freed.

Published rows are immutable spans and remain stable through later RW/full
warmup. The Hungarian matcher can borrow their immutable storage. The measured
increment therefore includes counted construction, exact allocation, removal of
the old late empty instruction walk and span borrowing. Total percentages do
not isolate any one of those mechanisms. cross_info and field storage are unchanged.

Debug and DIAGNOSTICS builds validate segment endpoints and replay final fill
lengths. Normal release omits that validation-only replay; size overflow and
whole-array write checks stay active. This follows the concrete finding in the
[source review](COMPACT-CALLERS-REVIEW.md). Pro inspected b28699b, not the final
e3ec362 correction, and did not run tests or benchmarks.

- 66 native validation commands pass: 13 normal, 36 diagnostic/isolated,
  and 17 sanitizer commands. Coverage includes component arithmetic and
  no-exceptions checks, inverse/candidate integration, existing relation checks,
  and the frozen complete 29-query tiny/mixed/giant invocation results.
- The authored five-DEX caller oracle covers 33 methods and 31 edges, including
  duplicate definitions/instructions, unresolved-before-resolvable references,
  reverse ClassDef order, empty DEX rows and two successful zero-count bindings.
  All five control/compact/isolated/sanitized dumps agree byte for byte. Its
  raw rows are independently validated; the full FlatBuffer tail is compared
  between implementations rather than independently decoded by Python.
- One/four workers, cold/late/full initialization, concurrent full warmup and
  a caller query queued behind an active guard validate publication, released
  capacity and stable row addresses through later RW/full initialization.
- 12 further caller/invocation commands on mostly-empty and one-source fixtures
  pass with identical complete dumps for control, compact and ASan/UBSan.
- Normal and sanitizer string/batch checks match their independent 90/36-query
  oracles on small and wide fixtures, alongside the prior Small control.
- Six QQ verification runs pass the frozen complete results and flow. The
  subsequent 72 measured JVMs retain equal result counts and flow; verification
  and timing use the same native/input/JDK/JAR/adapter/probe identities.
- `:dexkit:cmakeBuild`, `:dexkit:jar`, all 71 JVM tests (zero failures/errors/
  skips), and `:dexkit-android:assembleRelease` pass with Small plus callers.
  Android builds cover arm64-v8a, armeabi-v7a, x86 and x86_64. Host ASan/UBSan
  and 32-bit Android compilation do not substitute for Android device execution.

## Evidence and reproduction

[Structured summary](evidence/compact-callers/v2/summary.json),
[validation summary](evidence/compact-callers/v2/validation-summary.json),
[raw evidence](evidence/compact-callers/v2/raw-evidence.tar.gz),
[archive manifest](evidence/compact-callers/v2/manifest.json), and
[per-file integrity](evidence/compact-callers/v2/integrity.json) contain the
accepted plans, commands, samples, reports, logs, fixture generators, bounded
fixtures, source snapshots and artifact identities. QQ APK and compiled
native/JVM/Android outputs are omitted. Obtain the same QQ corpus separately.

The archive's `drivers/build.py`, `validate.py`, `verify_qq.py`, `validate_extra.py`,
`gradle.py` and `measure.py` describe the exact run. Paths are machine-specific
and must be adjusted in a new reproduction. Keep compiler/SDK/JDK versions,
normal diagnostics-OFF settings and input hashes equivalent. Re-run full ordered
verification before accepting newly compiled binaries for timing. `measure.py`
freezes the finite plan before its first measurement.

The native implementation is fixed at e3ec362. Subsequent fixture/report commits
do not change the timed engine. The CMake option is
`DEXKIT_EXPERIMENT_COMPACT_CALLERS`; Gradle uses
`-PexperimentCompactCallers=ON`, alongside the explicitly listed Small options.
