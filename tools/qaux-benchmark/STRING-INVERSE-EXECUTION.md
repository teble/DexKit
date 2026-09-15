# String inverse index experiment

Status: implementation complete, validation in progress, based on `9432879`.

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
- Trace, sanitizer, and standalone builds succeeded. Their execution checks,
  JVM/Android validation, source review, and formal timing remain pending.

The reverse index stays bridge-owned; candidate bitmaps are query-local. The
16 MiB budget applies to bitmap planes, with keyword-map metadata separate.
Top-level candidate traversal is limited to unscoped, non-findFirst queries.
Local root methods cannot carry cross-DEX bindings: the binding writer visits
only undefined owner types, which the existing root finder already excludes.
Recursive matching retains its original cross-DEX resolution, and the scoped
predicate shortcut checks the owning DEX, entity kind, and exact matcher vector.
