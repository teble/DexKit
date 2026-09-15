# Completed ordered optimization experiments

All six mechanisms in NEXT-ROUND-EXECUTION.md are complete: 160 finite sweeps
and 1,920 fresh-process samples, including independently ordered confirmation.
Every candidate remains behind default-OFF options. These are independent
comparisons against the nine-switch control, with rebuilt same-source controls
where needed; no combined candidate is selected and gains are not added.

The main findings below use confirmation results. Individual reports retain
both batches, paired intervals, complete artifacts and unfavorable results.

| Mechanism | Supported finding | Boundary or cost |
| --- | --- | --- |
| One ordinary Equal string | DIRECT and ID reduce the real QQ repeated Equal API by 75.27% and 79.18%; eleven-pass lifecycle improves 11.27% and 9.75% in their separate control comparisons. | Actual ID-versus-DIRECT pairing gives another 15.09% Equal API improvement, but no confirmed extra whole-QQ gain. ID loses 16.11% on the sparse native lifecycle. |
| One ordinary StartWith string | Both paths avoid AC work. In direct pairing, ID improves long late-prefix lifecycle by 45.60% over DIRECT. | ID regresses sparse lifecycle 25.63%; broad positive prefix has no confirmed extra benefit. Original QQ has no explicit StartWith, so its PREFIX-on results are guards. |
| Batch boolean containment | Replacing the intersection vector with includes improves the real 149-group QQ repeated API by 4.45%. | Whole-QQ lifecycle and peak remain unresolved; empty-search-set guards do not exercise containment. |
| One using-field requirement | Concentrated long field rows improve native lifecycle roughly 26-35%; short rows improve too. | No confirmed QQ gain. Retain eleven-pass confirmation lifecycle +2.37% and repeated nested-class peak +0.97%. |
| Contiguous forward-field rows | Original QQ lifecycle improves 8.96% / 1.48% at one/eleven passes; peak improves 3.09% / 3.44%. | Long native rows repeat peak increases of roughly 45-62%. Warm-query increases and unresolved getter/multiple/tail effects remain. |
| Skip deferred RW instruction walk | Real final QQ RW API improves 38.47% / 33.32% after one/eleven passes; one-pass whole lifecycle improves 3.32%. | Eleven-pass whole lifecycle and peak remain unresolved. Preserve the native forward-only confirmation increase and following-writer increase. |

Equal DIRECT is the smaller implementation; ID additionally provides pool-level
rejection and integer/range matching, with query-plan construction costs. The
bounded experiments do not select an automatic crossover threshold. Equal and
StartWith paths apply to one supported, nonempty, case-sensitive ASCII
requirement in ordinary find APIs. Contains, multiple requirements, unsupported
patterns and every batch API retain their appropriate existing paths.

Contiguous field storage includes both the storage change and borrowing target
rows in the existing matcher; its improvement cannot be attributed only to
smaller row headers. Deferred RW skipping changes no table layout and directly
saves only a first eligible instruction scan. Those are separate experiments,
with COMPACT_FIELDS disabled in the RW comparison.

## Reports and reproducible evidence

* [Equal and StartWith results](SINGLE-STRING-RESULTS.md),
  [source review](SINGLE-STRING-REVIEW.md), evidence under
  evidence/next-round/strings-equal and strings-prefix.
* [Batch containment results](BATCH-CONTAINMENT-RESULTS.md),
  [source review](BATCH-CONTAINMENT-REVIEW.md), evidence under
  evidence/next-round/batch-containment.
* [Single-field results](SINGLE-FIELD-RESULTS.md),
  [source review](SINGLE-FIELD-REVIEW.md), evidence under
  evidence/next-round/single-field.
* [Contiguous fields results](COMPACT-FIELDS-RESULTS.md),
  [source review](COMPACT-FIELDS-REVIEW.md), evidence under
  evidence/next-round/compact-fields.
* [Deferred RW results](RW-WALK-RESULTS.md),
  [source review](RW-WALK-REVIEW.md), evidence under
  evidence/next-round/rw-walk.

All native changes complete their required desktop/JVM and four-ABI Android
build checks, together with ordered-result, independent fixture and sanitizer
coverage detailed per report. Fixed-source reviews use the authorized
[Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3).
The review records distinguish actual source scope from locally executed
checks and measurements.

Timing is on macOS ARM64 with the frozen QQ APK, four threads and unchanged
query adapters. Android builds are not Android performance measurements.
Intervals are exploratory paired bootstrap intervals from six pairs; they
do not guarantee absence of regression. Lifecycle includes preparation,
complete output handling, destruction and close. Diagnostics and all internal
metrics are disabled for formal timing, and no heavy local validation/archive
work runs concurrently. Proprietary APK and binary artifacts are omitted from
the repository evidence; their identities and commands are recorded.
