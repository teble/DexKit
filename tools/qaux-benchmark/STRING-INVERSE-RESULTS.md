# Compact string reverse references: final comparison

The corrected implementation at `cc9f893` improves the repeated QQ workload, but is not uniformly faster. Keep `DEXKIT_EXPERIMENT_INVERTED_STRINGS` default OFF. It adds a compact bridge-owned reverse index, unique referenced-string AC scans, and candidate bitmaps for ordinary and Batch queries. The real QQ method-name-selective query and the one-DEX nested guard both regress.

The final comparison has 26 sweeps / 312 fresh process samples. The earlier `3ccb765` prototype has another 24 sweeps / 288 samples and is archived separately; its results are not substituted for the corrected engine. See [source review](STRING-INVERSE-REVIEW.md) and [execution record](STRING-INVERSE-EXECUTION.md).

## Controls and measurement

- Control: the existing nine options (LAZY_DIRECTORIES, COMPACT_STRINGS, NEGATIVE_STRINGS, STRUCTURAL_DESCRIPTORS, DESCRIPTOR_FAST_HITS, RAW_INTERFACES, FIELD_IDENTITY_SPLIT, COMPACT_INVOKES, SINGLE_RELATION). Candidate adds INVERTED_STRINGS only. The preceding six experiments remain OFF.
- macOS ARM64, four engine workers, frozen QQ 9.3.55 and QAux query corpus, JBR 17.0.6. QQ replays all 149 string groups and ten feature chains, for one or eleven passes. Native fixtures run sixteen positive/negative pairs per process.
- Every workload has six balanced randomized A/B process pairs, then an independently seeded confirmation batch. No build, validation or diagnostic workload runs during measurement. OS file caches are not flushed; this is a local machine measurement, not an Android runtime measurement.
- Percentages below are medians of paired relative changes; brackets are exploratory paired bootstrap 95% intervals. Negative is faster/smaller. Absolute before/after numbers are marginal medians, so their ratio need not equal the paired percentage. An interval crossing zero is unresolved, not proof of equality.
- Lifecycle includes creation, query preparation, API work, result handling/destruction and close. Stage timings stop before adapter postprocessing; native result destruction is inside the API leg. Process peak is macOS peak physical footprint and includes the JVM where applicable.

## QQ end-to-end results

| Metric | Main paired change and interval | Confirmation paired change and interval | Confirmation control -> inverse |
|---|---:|---:|---:|
| 1 pass: lifecycle | -5.64% [-8.14, -3.05] | -4.67% [-6.09, +0.49] | 1492.42 -> 1420.80 ms |
| 11 passes: lifecycle | -20.37% [-24.63, -15.31] | -19.24% [-20.60, -16.81] | 5550.54 -> 4513.64 ms |
| 1 pass: API total | -9.78% [-12.61, -4.52] | -8.81% [-9.91, -7.83] | 913.85 -> 831.57 ms |
| 11 passes: warm API total | -25.42% [-31.81, -20.88] | -24.35% [-26.74, -20.77] | 4046.52 -> 3073.17 ms |
| 1 pass: process peak | +0.48% [+0.14, +0.69] | +0.17% [+0.01, +0.81] | 1530.54 -> 1536.41 MiB |
| 11 passes: process peak | +1.70% [+1.05, +2.17] | +1.63% [+1.25, +1.89] | 1540.84 -> 1562.24 MiB |

Eleven-pass lifecycle improvement repeats; the corrected single-pass lifecycle confirmation crosses zero. Single-pass API work improves, but the complete lifecycle also includes creation and other work. Peak increases repeat: the eleven-pass marginal median difference is about 21.40 MiB, substantially larger than the retained index arrays alone.

| QQ stage (11-pass run) | Main paired change and interval | Confirmation paired change and interval | Confirmation control -> inverse |
|---|---:|---:|---:|
| 149-group Batch: first API | -12.20% [-13.74, -6.73] | -12.38% [-13.79, -10.01] | 211.940 -> 184.662 ms |
| 149-group Batch: warm API per pass | -30.74% [-33.27, -25.78] | -29.52% [-30.47, -27.88] | 119.204 -> 84.163 ms |
| Feature chains: first-pass API total | -5.03% [-10.95, +0.09] | -10.19% [-11.24, -7.03] | 706.443 -> 640.684 ms |
| Feature chains: warm API total per pass | -23.50% [-31.23, -18.63] | -22.12% [-25.28, -17.82] | 285.550 -> 223.147 ms |

The first Batch API includes initial forward-cache and reverse-index work; it is not a warm index lookup. The chain aggregate includes string and non-string queries. The implementation changes candidate traversal, repeated matching, scheduling and Batch negative-memo use, so these gains cannot be attributed only to fewer AC scans.

## Real QQ selective-query regression

The `AutoReceiveOriginalPhoto_cache_miss / nt_on_init_view` query uses:

```java
MethodMatcher.create().name("onInitView")
    .usingStrings("rootView", "em_bas_view_the_original_picture")
```

| Stage | Main paired change and interval | Confirmation paired change and interval | Confirmation control -> inverse |
|---|---:|---:|---:|
| First call | +311.63% [+267.28, +347.58] | +297.47% [+274.10, +342.75] | 10.039 -> 40.159 ms |
| Warm call per pass | +296.23% [+279.90, +322.39] | +309.65% [+296.26, +325.47] | 9.788 -> 39.904 ms |

The old matcher evaluates the method name before usingStrings. The new path constructs string candidates before those remaining predicates. This source ordering explains why a strong name filter can make the full referenced-string scan unnecessary work; no isolated attribution experiment claims that it accounts for every millisecond. This repeated real-query regression is retained despite the improved aggregate. A small `find_avatar_listener` first-call regression appears only in the main batch (about 0.13 -> 0.15 ms); its confirmation is unresolved.

## Native workloads and unfavorable case

| Workload | Main lifecycle change and interval | Confirmation lifecycle change and interval | Confirmation control -> inverse (ms) | Confirmation peak change and interval |
|---|---:|---:|---:|---:|
| Long Equal, shared references | -94.43% [-94.51, -94.22] | -94.42% [-94.46, -94.36] | 710.192 -> 39.532 | -6.44% [-7.09, -5.87] |
| Long StartWith, distinct extensions | -94.34% [-94.43, -94.23] | -94.41% [-94.55, -94.35] | 709.316 -> 39.822 | -6.85% [-8.40, -4.33] |
| Class Equal | -99.03% [-99.09, -98.98] | -99.08% [-99.11, -99.04] | 670.755 -> 6.218 | +5.38% [+5.14, +5.87] |
| Contains, shared references | -94.00% [-94.06, -93.95] | -94.06% [-94.15, -94.01] | 699.552 -> 41.825 | -2.19% [-3.58, -1.08] |
| Contains, distinct extensions | -86.94% [-87.07, -86.72] | -86.93% [-87.16, -86.87] | 704.769 -> 92.004 | -0.67% [-2.18, +1.06] |
| Multiple root strings | -94.23% [-94.27, -94.16] | -94.27% [-94.36, -94.22] | 730.811 -> 41.850 | -2.61% [-3.29, -0.15] |
| Exact declaring-class sparse guard | -5.25% [-12.65, +1.41] | -2.06% [-5.09, +4.33] | 5.530 -> 5.456 | -0.07% [-0.13, +0.07] |
| Overlapping Batch methods | -98.15% [-98.18, -97.54] | -98.14% [-98.20, -98.07] | 2141.710 -> 39.660 | -0.72% [-2.91, +2.85] |
| Overlapping Batch classes | -98.68% [-98.69, -98.63] | -98.66% [-98.67, -98.65] | 1679.158 -> 22.443 | +0.98% [-4.39, +3.14] |
| Batch all-miss guard | -2.33% [-4.45, -0.05] | -0.57% [-2.31, +0.38] | 13.167 -> 13.107 | +6.24% [+4.36, +7.93] |
| One DEX, broad root + nested allOf | +26.51% [+23.86, +28.39] | +25.56% [+23.73, +29.12] | 1780.988 -> 2240.611 | -13.58% [-14.80, -12.04] |

These generated workloads intentionally contain repeated or overlapping strings and are not QQ-wide speed predictions. Sparse-query lifecycle remains unresolved. Batch all-miss lifecycle improvement does not repeat decisively, while its peak increase does (+7.14% main / +6.24% confirmation). Class Equal peak also increases in both batches (+5.38%). Several other peak changes are unresolved or change direction; all metrics and samples remain in the raw summary.

The one-DEX workload contains 4500 methods with sixteen LONG references each. The root Contains condition admits every method; two allOf children have separate matcher vectors and still run the original per-method string matcher. Control uses five ranges of at most 1000 IDs with four workers; the inverse path uses one task for that DEX. Its remaining matching work therefore loses range parallelism.

| One-DEX nested leg | Main paired change and interval | Confirmation paired change and interval | Confirmation control -> inverse (ms) |
|---|---:|---:|---:|
| All sixteen positive APIs | +67.61% [+62.73, +70.07] | +66.31% [+62.89, +68.78] | 1345.663 -> 2239.029 |
| All sixteen absent APIs | -99.78% [-99.79, -99.77] | -99.78% [-99.80, -99.76] | 432.764 -> 0.964 |
| First positive + absent pair | +22.21% [+15.62, +27.77] | +21.37% [+18.80, +23.07] | 117.276 -> 142.330 |
| Remaining fifteen pairs | +26.79% [+24.47, +28.49] | +25.86% [+24.09, +29.56] | 1663.096 -> 2097.849 |

The absent query never executes the expensive children. Its near-elimination masks part of the positive regression; the positive leg is about 66% slower, versus 26% for the complete lifecycle. This still combines changes to root scanning, scheduling and output, rather than isolating parallelism alone. A post-measurement checker reuses the exact query builder and unchanged Core archives: all sixteen positive calls return the complete ordered 4500 IDs/descriptors, and all negative calls return zero. All 24 timed samples also match the independently calculated aggregate result identity.

## Storage and mechanism verification

- Retained inverse array capacity: 5,609,448 bytes (5.34959 MiB) over 41 DEX files; 949,982 referenced string IDs, 2,001,450 distinct method/string edges, 713,828 singleton rows. All per-DEX counts and capacities equal the earlier independent code census. Forward rows remain intact.
- Largest per-DEX construction scratch request: 784,736 bytes (0.74838 MiB). Sum of the four largest DEX scratch sizes: 2,822,960 bytes (2.69218 MiB). These are logical array amounts, not concurrent process-peak measurements.
- All 1353 logged AC jobs (33 per DEX in the eleven-pass replay) scan exactly that DEX's referenced distinct string IDs. Their total is 31,349,406 string visits. QQ has no optimized class-query jobs in this replay; generated fixtures cover that path.
- Largest observed AC bitmap budget formula: 2,733,456 bytes per DEX task. The corrected guard limits requested bitmap storage to 16 MiB per task in both construction and consumption, including the type-sized union. Keyword maps, vector headers and allocator effects are outside this bitmap budget.
- The nine-option control already memoizes duplicate negative Batch strings. Raw forward reference counts therefore must not be presented as the old engine's actual AC call count. Index array capacity is not the process-peak increment.

## Validation and reproducibility

- Both small and wide string-pool fixtures: six native configurations, 90 ordinary queries and 36 batches per fixture/configuration, independent raw-row predicates, complete ordered FlatBuffer equality, cold/full/repeated/concurrent schedules.
- Index representation: 51 layouts, 5100 interval unions, 16/32-bit boundary, duplicate edges, empty/rank-word cases; eight added budget/overflow shape checks.
- Integration: one root-only control plus seven root/child cases; same-vector and opposite-truth cross-DEX trap; four fresh concurrent bridge starts after forward-only prewarming. Control, candidate, ASan/UBSan and standalone diagnostic variants pass, with one construction per DEX in each concurrent window.
- Real 10,000-keyword budget rejection before index construction; successful small query afterward; repeat after full cache. Reversed ClassDefs retain complete ordered results.
- Corrected normal control, normal candidate and diagnostic candidate each pass eleven full frozen QQ replays and query-flow verification.
- Corrected Gradle cmakeBuild, jar, test and Android assembleRelease pass: 71 JVM tests and arm64-v8a, armeabi-v7a, x86, x86_64 in the AAR. All five CMake caches contain exactly the nine control options plus INVERTED_STRINGS. Android runtime timing was not performed.
- Two complete Pro source reviews: the budget defect was fixed; the final review found no new source blocker. First-concurrent coverage does not claim a forced simultaneous pause inside call_once. Allocation-failure injection was not performed; OOM is not a general fallback promise.

Corrected normal libraries:

```text
control  1406de12c38e356b1771209f2006c73a189975e4f90c892dcf75f48c3b4fab16
inverse  2c73406611fc8eba1d5541bd47187523c1cd1b1676a33a87cd2a1b7650e741ba
```

Native snapshots were built from committed bases plus recorded patches. Source reconstruction checks match every snapshot to `3ccb765` (prototype) or `cc9f893` (corrected), including the first prototype's then-untracked Core files. The control library remains byte-identical to the preceding nine-option control.

- [Corrected summary](evidence/string-inverse/v2/summary.json), [diagnostic summary](evidence/string-inverse/v2/diagnostics-summary.json), [source identity](evidence/string-inverse/v2/source-identity.json), [archive manifest](evidence/string-inverse/v2/manifest.json).
- [Earlier prototype summary](evidence/string-inverse/v1/summary.json) and [archive manifest](evidence/string-inverse/v1/manifest.json). Proprietary QQ APK, native/JVM/AAR binaries are excluded; synthetic fixtures, source, commands, identities and raw observations are retained.

Ordinary fallback still retains the one-task-per-DEX routing decision. Selective non-string predicates and broad expensive remaining conditions need a query-planning/scheduling policy before enabling this path generally. The current experiment remains default OFF.

## Follow-up: equivalent AND flattening

The user asked whether avoiding allOf removes the heavy-case regression. A separate query-shape comparison keeps the engine and predicates fixed: flattening these string AND nodes reduces the inverse workload from about 2238 ms to 36.4 ms (paired -98.38%), while the old control improves about 42%. Removing a condition is not required; it would change results on the small fixture (4 -> 45). See [the detailed follow-up](STRING-INVERSE-SHAPES.md) for six additional sweeps / 72 samples, exact equivalence checks, positive/negative legs and the anyOf distinction.

## Follow-up: conservative admission

The subsequent `7d536bf` experiment protects the original pipeline for queries
with additional conditions and independently tests one-posting ranges on an
already published index. It does not change candidate/residual scheduling.
See [the admission results](STRING-ADMISSION-RESULTS.md) for 44 sweeps / 528
samples, restoration of the known regressions, and the substantial prior
inverse gains that the conservative policy gives up on other shapes.
