# Ordinary single-string review

The authorized [Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)
completed its source review of `ea22702c39885ea02cd25c52382ab207f7d42385`
after 8m 27s. It read the single-string index/trace/query-context headers,
matcher/find paths, CMake and both Gradle files, the string query/check/workload
tools, fixture generator, validator and execution plan. This is source review,
not an independent execution of the local tests or a review of later commits.

## Confirmed issues and corrections

* `DexItem::dex_id` is 32-bit, but the prototype's two helper parameters were
  16-bit. DEX 65536 could reuse DEX 0's published range. Commit `1b9dd37` keeps
  the ID 32-bit throughout and extracts the existing per-DEX publication code
  into `single_string::DexRanges`. The actual component is exercised with
  different pools at IDs 0 and 65536, eight simultaneous callers, stable
  published addresses, out-of-range IDs and an empty directory. It does not
  claim to expand older cross-DEX relation APIs beyond their existing domain.
* The original bulk filler recreated the lowercase final `e` once per 20
  methods. This was also found locally before any formal timing. The generator
  now uses uppercase final bytes and asserts every bulk row's exact witness
  positions. Regenerated early/late/miss inputs are distinct from the initial
  faulty fixtures; no timing conclusion uses those initial inputs.

## Validated design and evidence limits

The review found no additional ownership/publication counterexample in the
read paths. The plan and borrowed FlatBuffer view belong to QueryContext;
per-thread fast indexes rebind by query ID. Each DEX range is written under
its mutex and published with release/acquire, including the invalid/fallback
completion state. An empty valid range makes only that atomic predicate false.
Original schema Equal, ASCII bytes 1..127, nonempty and case-sensitive gating
is preserved. SimilarRegex, Contains, multiple requirements and unsupported
values keep their existing paths. Batch and FindField are excluded by the
query-kind gate. StartWith remains independently disabled for the Equal phase.

The decoder validates only code units reached by binary-search comparisons;
it assumes a valid, sorted DEX pool. This is not arbitrary malformed-input
equivalence or complete pool validation. The complete result checks cover
missing values, not literal null table entries: the current DSL uses non-null
StringMatcher elements, and FlatBuffers offset-vector Get does not represent
an offset-zero slot as a null pointer. No malformed offset is presented as a
valid public query in the tests.

The small checker has separate concurrent QueryContexts. The additional
1,500-method-per-DEX diagnostics exercise multiple tasks within each query
and DEX: one shared plan per query, one range construction per visited DEX,
and matching result counts. The eight-caller component test separately checks
the publication boundary without constructing 65,537 real DEX files.

The validator now checks the complete `(classes, variant)` key set. Reusing
control output requires its prior successful validation, matching fixture,
native and checker executable identities, and unchanged serialized bytes.
The prior small/wide outputs were independently rechecked and consistent;
these changes do not establish that earlier results were contaminated.

Trace counters measure the instrumented ordinary leaf matchers. They exclude
the separate prefilter and batch AC work; task DEX labels must not be mistaken
for the pool visited by a nested cross-DEX matcher. `plan_bytes` counts owned
plan/entry storage, excluding allocator overhead and shared cache containers.
Formal timing excludes trace and internal metrics, includes query preparation,
result destruction and close, and does not infer speed from diagnostic runs.

## Verification after the correction

The final Equal control is byte-identical to the previous nine-switch
combination. DIRECT is also unchanged from its previously checked binary;
ID has a new native hash. The wide independent 68-query ordered-result oracle
and the ID ASan/UBSan run pass, including the 32-bit DEX isolation test. Both
experimental Gradle configurations pass native build, JAR, 71 JVM tests and
four-ABI Android AAR assembly; the desktop native hashes equal the immutable
measurement artifacts. Both candidates pass 11 frozen QQ replay rounds with
the same JAR/JDK/probe used by measurement. Android runtime performance has
not been measured.

The initial wrapper invocation failed because `gradlew` is tracked without
its executable bit. Running the same wrapper through bash then exposed the
missing SDK environment in this worktree. Explicit existing SDK/JDK paths
resolved configuration; the successful runs above supersede those failed
launches. No local.properties or generated sources were edited.

## Prefix extension and completed Equal evidence review

The same authorized Pro conversation completed a further review of fixed
`d67fe1c4a5cb5ac27995b31905811d8463cffa5c` after 7m 31s. It read the specified
single-string source, fixture/query tools and reports, and sampled the Equal
summary and SettingEntry head-to-head evidence. It did not independently run
the 576 measurements or certify later results. No additional source blocker
was identified in that review.

The review rechecked 32-bit DEX isolation, mutex publication, query ownership
and prefix lower/upper bounds. `DexRanges::Get` relies on its caller keeping
the pool and predicate fixed for each matcher-owned instance; it does not
revalidate that contract on every call. The ordinary-query gate still excludes
all batch APIs, multiple requirements, SimilarRegex and unsupported patterns.

The new fixture uses distinct proper extensions of the long prefix. The broad
prefix matches every bulk row's references, not the entire DEX string pool.
The class and sparse workloads use a separate short prefix. Overlapping
multiple-prefix queries exercise unchanged AC fallback; they do not evaluate
a multi-prefix range algorithm. Pool and matched-range counters are summed
per query/matcher range construction, not globally deduplicated pool size or
method hit rate.

The review agrees with Equal's conditional result: ID has a reproducible extra
SettingEntry API benefit, without a consistently resolved extra whole-QQ
benefit, and sparse access favors DIRECT. ID's empty-range rejection also
avoids reference visits, so its benefit is not solely integer-versus-byte
comparison. Prefix reporting must separate positive and fully absent queries
and include lifecycle costs. QQ contains no explicit StartWith query and is
therefore an incremental PREFIX-on overhead guard against Equal-only builds.
