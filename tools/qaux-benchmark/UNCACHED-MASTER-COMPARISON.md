# Current uncached combination versus master

Direct paired measurements compare unmodified local master `1d936bd9efa38e00b7336ef112e73ba4048324e6`
with the existing Small14 optimization combination plus `UNCACHED_DESCRIPTORS`
and `RAW_DESCRIPTOR_LOOKUP`, built at `2be1a63f1ac9de56f6e5cd53123322388e598006`.
This measures the cumulative optimization. Cache removal alone remains the
separate comparison in [UNCACHED-DESCRIPTORS-RESULTS.md](UNCACHED-DESCRIPTORS-RESULTS.md).
Earlier master-relative percentages were not combined with newer percentages.

All values are Apple M1 / macOS host measurements using QQ 9.3.55 (41 DEX files),
the pinned QAuxiliary query corpus and four query workers. The experimental
uncached flag is enabled in the measured candidate; production defaults are unchanged.

## Direct results

The independent confirmation batch is the primary table. Times and peaks are
absolute medians. Changes are medians of within-pair percentages, so they need
not equal the ratio of the two absolute medians. Negative changes favor uncached.

| Scenario | Passes | Lifecycle s, master -> uncached | Lifecycle change, 95% interval | Peak MiB, master -> uncached | Peak change, 95% interval | Paired peak saving MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| normal | 1 | 2.493 -> 1.182 | -52.58% [-55.11, -52.06] | 2000.17 -> 1340.19 | -32.89% [-33.23, -32.79] | 657.74 |
| normal | 11 | 8.758 -> 4.022 | -54.03% [-55.26, -52.47] | 2013.22 -> 1369.36 | -32.06% [-32.24, -31.87] | 646.27 |
| tail | 1 | 2.627 -> 1.465 | -44.43% [-45.57, -41.52] | 2008.01 -> 1459.34 | -27.31% [-27.51, -26.70] | 546.99 |
| tail | 11 | 8.857 -> 4.443 | -49.56% [-52.12, -48.79] | 2020.59 -> 1482.43 | -26.57% [-26.77, -26.01] | 536.11 |

Main batch:

| Scenario | Passes | Lifecycle s, master -> uncached | Lifecycle change, 95% interval | Peak MiB, master -> uncached | Peak change, 95% interval |
| --- | ---: | ---: | ---: | ---: | ---: |
| normal | 1 | 2.573 -> 1.178 | -53.73% [-55.06, -52.99] | 2002.87 -> 1342.24 | -32.98% [-33.23, -32.74] |
| normal | 11 | 8.998 -> 4.083 | -54.54% [-55.62, -54.20] | 2017.28 -> 1368.89 | -32.09% [-32.54, -31.76] |
| tail | 1 | 2.575 -> 1.470 | -43.07% [-47.21, -42.45] | 2002.73 -> 1459.26 | -27.00% [-27.26, -26.82] |
| tail | 11 | 9.076 -> 4.418 | -51.38% [-52.90, -49.91] | 2019.17 -> 1484.71 | -26.35% [-26.70, -26.19] |

## Workload and interpretation

- **normal:** the unchanged QQ queries, run once or eleven times within one
  bridge lifetime. The normal workload leaves field reverse indexes unused.
- **tail:** the same queries followed by fetching readers and writers of
  `Lcom/tencent/mobileqq/data/MessageRecord;->msg:Ljava/lang/String;` before close.
  This forces complete reverse-field construction across all 41 DEX files.
  Both implementations reproduce the independent ordered oracle: 399 readers
  and 313 writers. The final operation is performed once, not once per pass.
- **Lifecycle:** create, the complete query sequence, the tail operation when
  present, and close. Library loading is outside this timed window.
- **Peak:** OS-reported process physical footprint, including the shared JVM
  overhead. It is not just the descriptor cache, Java heap, or Android PSS.
- Eleven passes use a single bridge and include the initial pass. They do
  not mean eleven separate application startups. OS caches were not flushed.

## Validation and fixed design

Four complete eleven-pass verification runs (two variants, two scenarios)
passed the frozen complete-result oracle before measurement. Both variants
use the same JAR, schema and managed source. The existing native Release
binaries use the same Apple clang 21 compiler, arm64 target, SDK, deployment
target and `-O3 -DNDEBUG`. Diagnostics and internal metrics are compiled out.

The predeclared plan contains eight sweeps / 96 fresh JVM samples. Each cell
contains six balanced randomized AB/BA pairs, repeated in an independently
seeded confirmation. Every successful sample is retained. No product edits,
product builds, additional correctness checks, tuning or archive compression ran
during timing. All frozen input hashes reconcile before and after timing,
and every paired percentage and absolute median was independently checked
against the raw samples. Bootstrap 95% intervals are exploratory six-pair
estimates without multiple-comparison adjustment. All four lifecycle and all
four peak comparisons have intervals excluding zero in both batches.

## Source and evidence

Native library SHA256:

- master: `03eddd050d473950dbfc2d1757f1f541f11e168036b4431024c8c343ad5ce004`
- uncached: `095cfb7f54fe184fd8c6cb122c70e1784898ea693908bbf9f77d530dd129ecad`

Enabled experimental flags in the candidate (all have `DEXKIT_EXPERIMENT_` prefix):

`COMPACT_CALLERS`, `COMPACT_INVOKES`, `COMPACT_STRINGS`, `DESCRIPTOR_FAST_HITS`, `FIELD_IDENTITY_SPLIT`, `INVERTED_STRINGS`, `INVERTED_STRING_RANGES`, `LAZY_DIRECTORIES`, `MOVE_FIND_RESULTS`, `NEGATIVE_STRINGS`, `RAW_DESCRIPTOR_LOOKUP`, `RAW_INTERFACES`, `SINGLE_RELATION`, `SKIP_EMPTY_CANDIDATES`, `STRUCTURAL_DESCRIPTORS`, `UNCACHED_DESCRIPTORS`.

The pre-measurement harness commit is `eee6433e223571452fd753ff22ad32f2aa10f739`.
Frozen plan SHA256: `8c5be76400677965f278123d6e13a0579886ee7e3a178a7d03ca03c82367d87f`.

The evidence directory [evidence/uncached-master/v1/](evidence/uncached-master/v1/)
contains the frozen plan, source identities, summaries and an archive with
the driver, harness, verification reports and all timed process records.
The archive excludes the QQ APK, DEX files, JARs and compiled binaries.
Its member hashes and archive hash are verified after writing. Reproduction
commands and exact native artifact identities are in the frozen plan.
