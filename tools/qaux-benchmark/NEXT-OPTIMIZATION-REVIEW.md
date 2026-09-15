# Further optimization opportunities

Follow-up: all five proposals have now been implemented as default-OFF
experiments and individually validated. See [the measured decisions and final
combination](FOLLOWUP-RESULTS.md). The source review below records the earlier
planning state and does not itself claim performance improvements.

The user requested another Pro review to ask whether optimization is exhausted.
The existing completed experiment phase is not evidence of an optimization
ceiling. The next baseline must be the measured H1/H2/H3 + R1-fast + R3
combination, not the original all-flags-off engine.

## Review source and boundary

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

Pro reported reading fixed `3a8abb5424549bfa5e01aafb63e2728d563ce652`, including
the final results, best5 artifact manifest, DexItem declarations/initialization,
Analyze, cross-reference aggregation, matchers, batch/output paths, related
getters and QueryReplay. The complete final response was read and copied.
It distinguished the current source from the measured best5 native artifact at
`2e8e13c`. It did not run tests or produce new timing measurements.

The local cross-check read the corresponding source and the completed best5
pre-close census. Logical capacities below exclude some allocator and object
costs; they are not process peak measurements, speed predictions, or additive
promises. The previous failure of one byte-arena layout does not rule out all
paged or indirect representations.

## Ranked candidates

| Priority | Candidate | Logical capacity opportunity in this corpus | Main counterexample |
| --- | --- | --- | --- |
| 1 | Separate field identity resolution from reverse field-use graphs | Up to 116.77 MiB if the workflow never consumes reverse rows | Forward query followed by reverse access/full warm-up, including concurrency and nested matchers |
| 2 | Dense atomic pointers to stable descriptor strings | About 97 MiB of index/publication capacity before new string-object/ownership costs | High descriptor coverage, short strings, repeated large output and hot lookup |
| 3 | Direct contiguous storage for method_invoking_ids | About 29.75 MiB of row headers; payload savings depend on actual growth capacity | A huge row, mixed sparse/dense rows and complex nested invoke matching |
| 4 | Borrow source-file metadata through raw ClassDef | About 10.59 MiB of stored views | Repeated whole-domain source-file matching and class output |
| 5 | A single-requirement path for invoke/caller matching | No persistent saving; avoids per-solve target copies and matching state | Tiny rows, expensive nested predicates, late matches and high-fanout misses |

### 1. Separate field resolution from reverse graphs

Confirmed locally: `Analyze(MethodMatcher)` requests both `kMethodUsingField`
and `kRwFieldMethod` when `using_fields` is present. The existing comment says
the second dependency supports cross-DEX traversal. `IsFieldMatched` actually
forwards through `field_cross_info`; the get/put matcher helpers immediately
return on a null matcher. QueryReplay has one `addUsingField` at
`optional_has_info`, testing the field name `anonymousExtInfo`, and no explicit
field get/put-method matcher or corresponding getter invocation was found in
that adapter. This static observation is not a runtime consumption count.

First validate actual row consumption by non-null/meaningful reverse matchers
and explicit getters, without counting the null-matcher early return. If the
reverse rows are unused, split three readiness domains: forward field uses,
field identity resolution, and reverse reader/writer adjacency. Forward queries
must still get the same cross-DEX field resolution; simply deleting the old
`kRwFieldMethod` request would be incorrect.

Reverse access in any nested DSL branch, the public getters, and full-cache
requests must still build the required graphs. Current PutCrossRef clears the
pending reference lists and currently records aggregate work only for rows
whose reverse payload already exists. Delayed reverse initialization therefore
needs a deliberate way to recover the original merge order and work items.
Identity readiness must not stand in for reverse-graph readiness. Preserve
local edges before appended cross-DEX edges, source order, duplicates, chosen
definitions and the existing unresolved-reference cursor behavior.

The decisive checks compare forward-first then reverse access against
full-first, including concurrent requests and unrelated/unused fields. If the
whole workflow later needs both reverse tables, memory reduction may only be
a timing shift, with additional coordination cost.

### 2. Dense atomic descriptor pointers

The current preferred cache has 4,092,900 optional string slots (124.91 MiB)
and about 3.98 MiB of publication storage, but materializes only 1,628 strings.
An 8-byte atomic pointer per member is about 31.23 MiB. A null pointer is the
cold state; stripe locking and a recheck protect construction, followed by
release publication. A hit performs one acquire and reads the stable string.
Retain cached-text input lookup, standard string character storage, immutable
published content and views valid until bridge destruction.

This avoids the previous two-level page lookup and byte-arena layout, while
adding a dependency on the string-object pointer and a separate allocation for
each materialized string object. Those costs can be substantial at high coverage,
especially with small-string contents. Cleanup scans the directory and deletes
only constructed objects. Measure the complete workload, not just lower slot
capacity or faster destruction.

### 3. Contiguous forward invocation rows

`method_invoking_ids` is populated during a single InitCache instruction walk.
Its rows are then read by caller construction, metadata getters and matchers;
they are not the reverse rows appended during cross-DEX aggregation. Start
with this table alone, following the existing H2 direct-append pattern:
size_t offsets, checked uint32_t lengths, and uint32_t IDs. Preserve row order,
repeated calls and local-ID interpretation. Use borrowed read-only ranges in
consumers, rather than reconstructing a vector for each lookup.

The 24-byte row headers can become 12-byte logical indexes on this target.
Do not count all 56.15 MiB of existing payload capacity as avoidable. Growth,
old/new buffer overlap and cold build cost must be measured. Keep callers and
field reverse-table flattening separate because their construction and merge
lifecycles differ.

### 4. Further raw side-table reuse

The `class_source_files` vector stores a string_view per type ID. A defined
class already has a raw ClassDef source_file_idx and an existing type_def_idx.
A helper can recover the strings view while preserving undefined-class,
missing-source and cross-DEX forwarding behavior. This removes stored views,
but introduces dependent reads on source matching and class output.

Pro also identified the proto_type_list side table (about 4.45 MiB) and code
pointer-to-offset conversion as separate experiments. No assumption that QQ's
observed maxima cover the complete legal input domain is justified; narrowing
must have a domain proof or checked fallback.

### 5. Single-requirement matching

The ordinary Hungarian constructor currently copies both vectors and allocates
pair state, p and vis. Start only with invoke/caller matching when exactly one
requirement is present. Walk targets in the existing order with the original
judge; preserve count constraints and Equal semantics. All multi-requirement
cases retain the existing one-to-one solver. Nested AND/OR/NOT predicates must
keep their semantics and required cache initialization.

Persistent memory saving is zero. Per invocation this can avoid target copies,
roughly one byte of pair state per target, an int per target, and visited state.
An additional branch can be a loss for tiny rows, while expensive nested
predicates may dominate the saved preparation work.

## Lower-priority output observations

The local review confirmed ordinary method/field Find creates Beans before
aggregate descriptor deduplication, and performs separate set contains and
emplace operations. Pro additionally identified temporary batch intersections.
Count duplicate Bean/parameter-copy volume and actual intersection allocations
before prioritizing streaming deduplication or boolean containment checks.
Preserve first-representative/output ordering; source redundancy alone is not
proof that these paths dominate the measured workload.

## Suggested next validation

Pro's first choice is consumption diagnostics for field reverse rows, followed
by a field-only dependency split if the observation supports it. Descriptor
pointer indexing is the next independent representation experiment. Each new
candidate should be compared incrementally with best5 using the existing full
result verification, one/eleven-pass lifecycle measurements, independent
confirmation and relevant adverse fixtures. Charge deferred construction,
growth, synchronization and cleanup. A confidence interval crossing zero does
not establish time non-inferiority.
