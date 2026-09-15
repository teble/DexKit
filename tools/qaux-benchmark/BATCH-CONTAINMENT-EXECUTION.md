# Batch containment experiment

Status: implementation and checks in progress, after the completed Equal and
StartWith experiments. The incremental control is the nine-switch combination;
all single-string switches are OFF in this phase.

Only the two batch group checks change: replace an intersection vector plus
size comparison with `std::includes`, keeping the same default string-view
ordering. Preserve the empty-search-set guard, normalization, duplicate keys,
AC scan, negative memo and complete result construction. The independent
`DEXKIT_EXPERIMENT_BATCH_STRING_INCLUDES` option defaults OFF.

Separate `DEXKIT_BENCHMARK_BATCH_TRACE` builds count eligible candidates,
candidates reaching the group loop, group checks and matched groups. Control
builds also count nonempty intersection vectors, final item counts and final
capacity bytes summed over those vectors. These are not allocator-call counts,
peak live bytes or CPU timing. Timed builds disable all diagnostics and metrics.

The native oracle uses independent encoded fixture rows and code-presence
parsing for 16 method/16 class batches, then checks complete ordered serialized
results across control and candidate. Cases include empty groups, mixed empty
groups, overlapping and repeated atoms, duplicate/empty keys, all literal
boundaries, SimilarRegex normalization, ASCII case folding, MUTF-8 values,
composite fallback, missing values and empty input filters. Cold, full-cache,
repeated and concurrent sequences preserve their original behavior.

Measure unchanged QQ one/eleven-round replays and high-overlap method/class
batches plus a no-hit method control. Each finite comparison uses six balanced
fresh-process pairs and an independent confirmation batch. The short-string
fixture has three DEXes, 1,500 methods per DEX, ten methods per class and four
references per method. Its 64 overlapping groups include four successful
groups and sixty partial matches, to expose temporary intersection work without
returning every group. The negative leg has no string witnesses. Keep creation,
preparation, serialization/destruction and close in the lifecycle boundary.

Run frozen QQ verification, host/JVM checks and four-ABI Android assembly,
plus focused sanitizer and standalone-macro checks. Review the source and
evidence in the authorized Pro conversation after a meaningful increment.
Do not combine this change with trie borrowing, bridge key initialization,
ordinary string ranges or any later field experiment.
