# Conservative string admission: results

The implementation at `7d536bf4264ab5c8deeaa4a2a3f5a4831c76576d` completes the two requested changes: conservative admission for ordinary string candidates, and an independent existing-index range exception. Candidate/residual scheduling, predicate hoisting and string-vector fusion remain deferred.

The known onInitView regression is removed, and the old 4500-method nested workload returns close to the nine-option pipeline. This is a conservative policy, not a universally faster planner: it gives up substantial previous inverse gains on wide metadata conditions and on some nested shapes. Both experimental options remain default OFF.

## Rule and controls

- **Control**: the existing nine options: LAZY_DIRECTORIES, COMPACT_STRINGS, NEGATIVE_STRINGS, STRUCTURAL_DESCRIPTORS, DESCRIPTOR_FAST_HITS, RAW_INTERFACES, FIELD_IDENTITY_SPLIT, COMPACT_INVOKES, SINGLE_RELATION.
- **Old inverse**: the frozen `cc9f893` inverse artifact from the preceding experiment, added to those nine options.
- **Admission (B)**: the same nine options plus INVERTED_STRINGS, with the new admission rule. An ordinary unscoped, non-findFirst query retains whole-DEX candidates only for root usingStrings with no other present constraint. Empty logical lists keep their existing no-op semantics; other present objects are conservatively protected. Batch is unchanged.
- **Small**: B plus INVERTED_STRING_RANGES. A guarded root may use an original single nonempty case-sensitive ASCII Equal/StartWith only after an acquire observation of an already published index, with at most one distinct method/string edge in its range **per DEX**. Two matching string IDs in one method count as two edges. Cold/building indexes use Legacy without probing the pool, building, or waiting.

The decision is frozen before changing slices and carried into workers. A worker cannot upgrade Legacy when its slice covers a full DEX or another query publishes the index. Raw keyword counts and existing table sizes preflight bitmap budgets. A pure-string index-build failure after admission retains the previous single-task fallback boundary. No general allocation-failure fallback is promised.

## Measurement

There are 44 sweeps and 528 accepted fresh-process samples: six balanced randomized pairs per sweep, then an independently seeded confirmation of every comparison. macOS ARM64, four engine workers, JBR 17.0.6, frozen QQ 9.3.55/QAux queries. QQ uses all 149 string groups and ten feature chains for one or eleven passes. Native workloads use sixteen positive/absent pairs. Builds, tests and archive creation do not overlap accepted timing runs; OS file caches are not flushed. These are desktop measurements, not Android runtime measurements.

Negative percentages mean faster/smaller. Percentages are median paired relative changes, with exploratory paired bootstrap 95% intervals. Absolute figures are marginal medians, so their ratios need not equal paired percentages. An interval crossing zero is unresolved. Warm native setup includes the explicit query that builds the index and its destruction; API legs include result consumption/destruction, and lifecycle includes query destruction and close.

## Known regressions and restoration

| Metric / comparison | Main change and interval | Confirmation change and interval | Confirmation before -> after (ms) |
|---|---:|---:|---:|
| onInitView warm: old inverse -> B | -75.89% [-76.41, -75.25] | -76.07% [-76.58, -75.90] | 37.691 -> 9.023 |
| onInitView warm: control -> Small | -1.66% [-4.04, +1.50] | +1.19% [-1.92, +4.24] | 9.037 -> 9.114 |
| 4500 nested lifecycle: old inverse -> B | -20.80% [-22.93, -18.97] | -20.83% [-24.20, -20.29] | 2200.695 -> 1747.479 |
| 4500 nested positive APIs: old inverse -> B | -39.62% [-41.59, -38.36] | -39.89% [-42.72, -39.58] | 2198.804 -> 1319.960 |
| 4500 nested lifecycle: control -> B | +0.60% [+0.06, +3.61] | +2.52% [-0.22, +3.29] | 1702.998 -> 1740.587 |
| 4500 nested positive APIs: control -> B | +2.30% [+0.06, +4.22] | +2.75% [+0.66, +3.84] | 1285.011 -> 1317.264 |

The former approximately 66% positive-leg regression is reduced to a small residual cost: confirmation is +2.75% against the nine-option baseline, while lifecycle is +2.52% with an interval crossing zero. This does not establish exact equality or zero overhead. The negative nested leg gives up the old inverse shortcut: old inverse -> B is 0.827 -> 423.391 ms over sixteen absent calls; control -> B is 420.873 -> 421.462 ms. Positive and negative effects must not be hidden inside the aggregate.

## Whole QQ workload

| Metric / comparison | Main change and interval | Confirmation change and interval | Confirmation before -> after (ms) |
|---|---:|---:|---:|
| Old inverse -> B, 1 pass | -1.30% [-3.23, +3.56] | -0.64% [-7.19, +6.07] | 1346.370 -> 1328.826 |
| Old inverse -> B, 11 passes | -6.64% [-8.26, -4.67] | -7.91% [-9.29, -5.79] | 4180.595 -> 3849.258 |
| B -> Small, 11 passes | -0.66% [-2.35, +2.78] | +0.45% [-1.03, +1.78] | 3866.326 -> 3860.574 |
| Control -> Small, 1 pass | -6.71% [-7.66, -2.51] | -7.73% [-8.46, -7.34] | 1370.379 -> 1258.393 |
| Control -> Small, 11 passes | -24.79% [-26.15, -24.10] | -25.25% [-26.59, -23.81] | 5154.163 -> 3861.511 |
| Control -> Small, warm 149-group Batch per pass | -27.93% [-29.18, -27.79] | -27.96% [-29.74, -26.92] | 110.678 -> 79.465 |
| Control -> Small, warm feature chains per pass | -32.79% [-33.47, -31.43] | -32.00% [-32.57, -31.16] | 263.940 -> 179.160 |

B improves the repeated workload relative to the prior inverse artifact; the one-pass B increment is unresolved. Overall control -> Small gains include the existing inverse optimization and must not be added to previous reported percentages. Diagnostics show **zero guarded SmallRange admissions in this QQ replay**: B -> Small has no established QQ latency benefit. Pure Equal/Prefix pool probes also moved from workers to the submitting thread, so the B increment is not merely the cost of an added Boolean branch.

## Cost of the conservative rule

The new fixture has 2250 unique-name methods, 2250 methods sharing one name, and five special methods. Bulk methods each reference their own long string sixteen times. This differs from the older 4500-method nested fixture, whose methods share the same long string ID.

| Old inverse -> B | Main lifecycle change and interval | Confirmation lifecycle change and interval | Confirmation before -> after (ms) |
|---|---:|---:|---:|
| Rare name at root | -97.61% [-98.01, -97.40] | -97.72% [-97.88, -97.58] | 144.597 -> 3.273 |
| Broad name at root | +287.61% [+280.89, +291.14] | +292.11% [+282.03, +299.31] | 159.404 -> 623.006 |
| Rare name in allOf | +304.76% [+300.73, +316.57] | +301.67% [+298.43, +306.37] | 216.749 -> 872.947 |
| Broad name in allOf | +285.71% [+280.42, +295.34] | +280.41% [+277.08, +288.40] | 232.858 -> 885.185 |
| Broad static flag | +399.86% [+393.24, +408.51] | +394.78% [+388.05, +531.96] | 176.943 -> 872.138 |
| Broad void return type | +387.82% [+381.38, +393.76] | +402.20% [+392.89, +413.45] | 179.498 -> 900.139 |

These are repeatable losses of previous inverse gains, not evidence that a name or flag is intrinsically selective. Broad-name positive/absent API totals increase 91.993 -> 359.895 ms and 65.939 -> 261.845 ms; both legs lose. A rare name inside allOf does not move ahead of the existing composite string prefilter, so B does not give it root-name short-circuit behavior.

The added control -> B broad-name reference is -3.62% main and -3.89% [-4.86, -2.52] confirmation (639.180 -> 615.608 ms): it finds no additional regression against that original pipeline. The 3.8-5.0x lifecycle losses in the table are the opportunity cost of abandoning a previously beneficial inverse path. A more selective policy would require evidence beyond field presence; this change does not add that planner.

## Existing-index range exception

B -> Small, confirmation. API figures total sixteen calls. The warm lifecycle includes the explicit index warmup, so it is separate from the already-warm API benefit.

| Case | Lifecycle change and interval | Positive API change and interval | Absent API change and interval | Positive API before -> after (ms) |
|---|---:|---:|---:|---:|
| Equal H=1, cold | +2.68% [-7.73, +22.01] | +4.52% [-9.11, +25.54] | +3.05% [-15.87, +66.57] | 0.940 -> 0.953 |
| Prefix H=1, cold | +1.96% [-10.06, +9.10] | -1.62% [-10.59, +11.84] | +5.49% [-15.24, +14.14] | 0.979 -> 0.987 |
| Equal H=1, warm | -22.95% [-26.37, -9.27] | -83.27% [-87.52, -71.50] | -85.10% [-89.38, -76.09] | 0.772 -> 0.131 |
| Prefix H=1, warm | -22.58% [-23.32, -17.23] | -86.18% [-87.20, -79.51] | -88.03% [-89.33, -81.99] | 0.918 -> 0.136 |
| Equal H=2, warm | -13.44% [-18.68, -4.56] | -7.33% [-39.32, +35.51] | -88.15% [-88.31, -79.00] | 0.942 -> 0.846 |
| Prefix H=2, warm | -10.67% [-12.86, +1.15] | -8.56% [-19.68, +0.99] | -86.35% [-86.82, -84.37] | 0.795 -> 0.724 |

Both H=1 warm improvements repeat: main lifecycle changes are -18.51% Equal and -21.19% Prefix, followed by -22.95% and -22.58% confirmation. Cold cases have no established benefit; the main cold Prefix slowdown does not resolve in confirmation. Cold repeats only warm the existing forward/matcher state and never build the inverse index in these guarded workloads.

H=2 positive queries remain Legacy. Their absent counterparts have H=0 and can use Empty, so a total improvement does not demonstrate an H=2 positive optimization. Prefix H=2 lifecycle confirmation is unresolved. A pure, unguarded long Equal workload also retains its prior performance: B versus old inverse is -0.71% main (unresolved), -0.74% confirmation.

## Memory and diagnostics

QQ control -> Small peak physical footprint increases +1.21% [+0.80, +1.59] in eleven-pass confirmation (1542.021 -> 1559.888 MiB); one-pass is +0.75% [+0.56, +0.89]. These peaks include the JVM and transient allocations. B versus old inverse, and Small versus B, have unresolved QQ peak differences. Native broad/flags/return peaks also increase relative to old inverse; their full observations remain in the summary. Warm H=1 native Small peaks decrease about 14% relative to B. There is no general memory-reduction claim.

The compact index arrays retain 5,609,448 bytes over 41 DEX files, unchanged from the prior representation: 949,982 referenced string IDs and 2,001,450 distinct method/string edges. The QQ trace records 902 AC jobs (22 per DEX), each scanning that DEX's referenced strings, versus 1353 jobs in the previous replay. It records 121 ordinary queries and 4851 per-DEX admission decisions. A diagnostic ready=0/postings=0 means not counted, not a proof of no postings.

## Validation, review and evidence

- Seven current native configurations plus the previous inverse artifact agree with the independent raw-row oracle and complete ordered bytes: 90 ordinary and 36 Batch cases on both existing small/wide fixtures; 58 admission cases in each of three states on the new single/multi-DEX fixtures. States are fresh, full cache with inverse still cold, and explicit inverse warmup.
- The index checker covers 51 layouts and 5100 intervals, including duplicate instructions, 16/32-bit payload boundaries, empty and rank-word boundaries. Range counts are checked against independent posting counts as well as method unions.
- All four diagnostic configurations check 348 method/class admission decisions, 30 explicit-empty-scope/findFirst restrictions, and 12 concurrent publication/frozen-submission cases across the two fixtures. They do not force every possible interleaving inside Build.
- Budget rejection before construction, successful later construction, reversed ClassDefs ordering, existing nested/cross-DEX and first-publication integration tests pass. ASan/UBSan and the standalone inverse/range configuration pass.
- The Pro review read the fixed `a55ba18 -> 7d536bf` increment and found no new production correctness blocker. It identified a warm Range cross-DEX coverage gap. The added checker uses the same root vector in a remote matcher at the same numeric method ID, asserts actual Range/Empty plans and real cross-DEX binding, and validates both false and true cases across five frozen artifacts. No engine change was needed.
- Gradle `:dexkit:cmakeBuild :dexkit:jar :dexkit:test :dexkit-android:assembleRelease` passed: 71 JVM tests, no skips/failures/errors, and arm64-v8a, armeabi-v7a, x86, x86_64. All five CMake caches enable exactly the nine options plus INVERTED_STRINGS and INVERTED_STRING_RANGES. Five eleven-pass QQ oracle/flow verification runs also pass.

Three harness setup corrections are preserved separately: initially building new artifacts under the previous evidence root before relocation with unchanged hashes; supplying the wrong fixture to the old TLS checker; and losing execute permission when copying an old workload. The corrected checks pass, and the executable-permission failure produced no accepted timing sample. Completed valid sweeps were retained. One additional broad-name baseline comparison per phase was added after observing the loss of previous inverse gains; its rationale is archived.

The source and timing inputs remain frozen at `7d536bf`. The two extra cross-DEX test sources were linked afterward against frozen archives; their identities are recorded separately. The nine-option control library is byte-identical to the previous control. The user's main checkout was not changed.

- [Results and all metrics](evidence/string-admission/v1/summary.json)
- [Validation](evidence/string-admission/v1/validation-summary.json), [diagnostics](evidence/string-admission/v1/diagnostics-summary.json), [source identities](evidence/string-admission/v1/source-identity.json)
- [Archive manifest](evidence/string-admission/v1/manifest.json), [execution policy](STRING-ADMISSION-EXECUTION.md)
- [Original Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)

The archive contains commands, raw observations, failures/corrections, validation records, synthetic fixtures and reproduction sources. It excludes the proprietary QQ APK and native/JVM/AAR binaries; their identities remain recorded.
