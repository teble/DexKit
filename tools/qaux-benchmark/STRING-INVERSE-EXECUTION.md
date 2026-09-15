# String inverse index experiment

Status: budget revision and integration checks complete; final measurements pending.
The first measured prototype is `3ccb765`, based on `9432879`.

The user requested an engine implementation and another benchmark after the
QQ string-reference census. Work remains on the existing isolated experiment
branch. The new `DEXKIT_EXPERIMENT_INVERTED_STRINGS` flag defaults to OFF.

1. Implement an immutable, lazily published reverse index from string IDs to
   distinct method IDs. Use presence/rank tables, separate singleton rows, and
   checked 16-bit method IDs with a 32-bit alternative. Keep forward references.
2. Scan only referenced, distinct strings with the existing canonical AC matcher;
   build keyword candidate bitmaps and intersect them per group. Map postings to
   declaring classes before intersections for class queries. Use pool ID ranges
   for eligible single ASCII Equal/StartWith queries. Preserve scoped, composite,
   empty, cross-DEX and result-order semantics, with bounded query scratch space.
3. Compare complete ordered results against the flag-OFF engine on synthetic
   string and batch fixtures and all QQ queries. Check lifecycle, publication,
   ID width, range boundaries, sanitizer builds, JVM tests, and all four Android
   ABIs. Request source review in the already authorized Pro conversation.
4. Freeze native artifacts. Compare nine-flag control against nine flags plus
   this flag only, with diagnostics disabled. Run six randomized pairs and an
   independent confirmation for QQ (one and eleven passes) and relevant string
   and batch workloads. Report cold cost, warm queries, lifecycle, peak footprint,
   regressions, confidence intervals, and measured index/scratch capacity.
5. Archive reproducible evidence, publish the result report and push only to the
   existing teble fork branch. No Android runtime speed claim from desktop data.

## Initial validation

- Both normal native builds succeeded. The flag-OFF native SHA-256 remains
  `1406de12c38e356b1771209f2006c73a189975e4f90c892dcf75f48c3b4fab16`, identical
  to the preceding nine-flag control. The candidate is
  `0ff44a62d13634d52dbbde0fd649baa8b60b98ee4b889af6b0704d27b42eda33`.
- The small and wide string-pool fixtures pass all 90 ordinary queries and
  36 batches against independent row oracles and complete ordered output bytes.
  Cold/full-cache, repeated and concurrent query sequences are included.
- The compact-index checker passes 51 layouts and 5,100 interval unions,
  including empty data, rank-word boundaries, duplicate references, and method
  table sizes 65,536 and 65,537 (16-bit and 32-bit storage respectively).
- QQ verification passes eleven complete replay passes on control, candidate,
  and diagnostic candidate. The diagnostic index has exactly 5,609,448 allocated
  array-capacity bytes across 41 DEX files: 949,982 referenced strings,
  2,001,450 distinct edges, and 713,828 singleton rows. This exactly matches the
  independent census. Largest build scratch is 784,736 bytes; largest query
  bitmap budget usage observed is 2,733,456 bytes per DEX task.
- All six builds pass these checks. JVM validation passes 71 tests, and the
  Android release AAR contains all four expected ABIs.
- The first prototype completed 24 sweeps / 288 samples, including independent
  confirmation. QQ lifecycle improvement repeated, as did an eleven-pass
  process-peak increase. Preserve this evidence under `string-inverse-v1`;
  final measurements will use the budget revision and an added one-DEX guard.

The reverse index stays bridge-owned; candidate bitmaps are query-local. The
16 MiB budget is per DEX task and applies to requested bitmap storage, with
keyword-map metadata and allocator effects separate. It checks both keyword
construction and group consumption, including the type-sized union after a
method batch. Single ASCII range candidates also check their bitmap request.
`bitmap_budget_bytes` is a formula value, not a measured capacity/peak census.
Top-level candidate traversal is limited to unscoped, non-findFirst queries.
Local root methods cannot carry cross-DEX bindings: the binding writer visits
only undefined owner types, which the existing root finder already excludes.
Recursive matching retains its original cross-DEX resolution, and the scoped
predicate shortcut checks the owning DEX, entity kind, and exact matcher vector.

## Source-review revision

The review of `3ccb765` found a concrete budget gap for a method batch with a
much larger type table. The revised checked helper rejects the reported
4096-method / 60000-type / 1-keyword / 32765-group counterexample, and tests the
exact accepted/rejected boundary plus extreme `size_t` inputs. It uses division
and subtraction before bounded arithmetic. Ordinary fallback keeps the already
selected one-task-per-DEX scheduling; this remains a performance limitation.

The new small fixture exercises eight root-plus-child queries, including a
local caller and a remote target with equal numeric method IDs and opposite
string truth values. The remote nested query deliberately reuses the root
FlatBuffer vector. Four fresh bridges prewarm only `kUsingString`, assert that
the inverse index is absent, and start ordinary Contains and Batch together.
They check complete results and one index construction per DEX. Control,
candidate, ASan/UBSan and standalone diagnostic builds pass these checks.

Additional cold/warm budget-fallback and reversed-ClassDefs tests pass on the
first prototype and are repeated for the revision. `class_method_ids` is already
sorted by InitBaseCache; the batch path retains that existing row order.
No allocation-failure injection has been performed, and no general OOM fallback
is promised.

The final finite matrix has 13 sweeps per batch, six pairs per sweep, plus an
independent confirmation (26 sweeps / 312 samples). It repeats the original
12 workloads and adds a one-DEX, broad-root, expensive-nested-condition case to
expose the loss of method-range parallelism. No build or diagnostic workload
runs concurrently with formal measurement.
