# Next round after the completed optimization stack

Status: planning only. No candidate below has been implemented or assigned a
new performance gain. The first priority is a narrow exact-string experiment
comparing the current AC path, direct comparison and a sorted-pool ID lookup,
preceded by focused attribution. Prefix ranges need their own workload;
batch containment is the next independent experiment.

## Review and baseline

The user requested another discussion with Pro about remaining opportunities.
The completed response in the [existing Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)
reports reading fixed `856b6b463b1b47f226ad32b729bfdb404beb15cd`: relevant
MASTER-COMPARISON, FOLLOWUP-RESULTS and FOLLOWUP-REVIEW sections, QueryReplay,
matcher, batch, InitCache, relation aggregation, Find result merging, Bean
serialization, executor, scheduler and ThreadPool code. It did not rerun the
archived measurements. The supplied stage shares are API wall-time evidence,
not a CPU profile. The complete final response was read and locally checked.
A second completed reply reviewed the user's sorted-pool proposal at the same
commit, including string_match.h, dex_utf8.cc, the AC header, prefilter and
reference consumers, and bridge batch code. That full reply was also read;
the ordering, matching and copy observations were checked locally.

Every new candidate must compare against the current nine-switch combination:
LAZY_DIRECTORIES, COMPACT_STRINGS, NEGATIVE_STRINGS, STRUCTURAL_DESCRIPTORS,
DESCRIPTOR_FAST_HITS, RAW_INTERFACES, FIELD_IDENTITY_SPLIT, COMPACT_INVOKES and
SINGLE_RELATION. These names carry the `DEXKIT_EXPERIMENT_` prefix. The current
native snapshot is `2347a204a16ecf3b0855189ee2c3e62b27e8c1cdff476146ecb7964e6d7422ba`.
Master remains a cumulative comparison, not the next incremental control.
The older NEXT-OPTIMIZATION-REVIEW describes five experiments already completed.

Existing limitations remain relevant: dense descriptor pointers regress at
full SSO coverage, raw source lookup regresses under hot source matching,
compact invokes can increase mixed-row output memory, and repeated reverse
field consumers have a cost relative to best5. None is erased by the cumulative
QQ improvements. A different mechanism can be investigated separately, but
there is no reason to repeat a rejected representation without new evidence.

## Evidence used to prioritize

For each of six current-combination processes in the confirmation QQ 11-pass
batch, the final ten passes were grouped by API stage. The following are
medians of each process's stage share, not an allocation or CPU breakdown:

| Stage | Share of repeated API wall time |
| --- | ---: |
| all_literal_targets / batch_strings | 29.51% |
| SettingEntryHook / find_processor | 18.13% |
| HideMiniAppPullEntry / legacy_string_groups | 13.43% |
| HideMiniAppPullEntry / nt_invoke_fallback | 10.95% |
| BlockPicByMd5_LoadImagePathV2 / find_method | 5.09% |

These values derive from the six `pair-*-current-combo/report.json` records in
`qq-master-normal-confirm-p11`, retained in the master-comparison archive.
The exact SettingEntry predicate is `addEqString("SimpleItemProcessor")`.
In a verified repeated pass, the 149-group batch returns 1,631 grouped results
(summing groups preserves cross-group multiplicity); SettingEntry, the
three-group legacy batch and the invoke fallback each return one result.
This supports measuring string work before treating result copying as the
dominant cost of every stage.

The current diagnostic census also retains about 112.39 MiB in forward
using-field rows and 160.04 MiB in callers. These are logical capacities,
including row headers and payload capacity, not measured achievable savings.
There are about 1.54 million forward-field buffers. The remaining descriptor
slots are large too, but the prior pointer experiment already demonstrates
that reducing directory bytes can add expensive full-coverage object costs.

## Ranked candidates

### 1. One exact using-string requirement

Confirmed in `dex_item_matcher.cpp::IsMethodUsingStringsMatched`: a simple
Equal requirement still uses the keyword cache, Aho-Corasick hits, a set and
an intersection vector. The direct `IsStringMatched` loop belongs to a
different fallback condition.

Start only with one nonempty, case-sensitive Equal requirement. The direct
candidate walks the existing using-string ID row and uses the existing exact
comparison, returning after the first witness. A second candidate resolves
the required string ID once per DEX and query, then compares integers in the
same row. Restrict that initial ID candidate to ASCII without embedded NUL;
see the sorted-pool correctness constraints below. Keep null/empty inputs,
composite string predicates, other modes and multiple requirements on their
existing paths. Do not change Analyze, the existing prefilter, candidate
order or the public API, and do not build a new global string index.

Cost: low. Candidate benefit is less scanning/preparation and temporary
allocation, not the entire 18.13% SettingEntry stage. Validate actual
SettingEntry plus early/late matches, all misses with equal-length common
prefixes, empty/MUTF-8 strings, case-sensitive controls and enclosing logical
groups. If the complete API does not improve, or a counterexample repeatedly
regresses, stop expanding the special case.

### 2. Batch containment without an intersection vector

Confirmed in both `DexItem::BatchFindMethodUsingStrings` and its class
counterpart: the intersection vector is used only to decide whether all group
keywords were present. A boolean containment operation such as `std::includes`
can use the same comparator and avoid materializing that result.

Cost: low. First change batch only. Preserve the existing empty-search-set
guard, empty-group behavior, keyword normalization, matching boundaries,
case behavior, group keys and result multiplicity/order. Keep the set, trie
and negative-string memo unchanged so this experiment measures one mechanism.

Count candidates that actually reach the group loop and temporary
intersection work before prioritizing a large rewrite. Compare the existing
149-group and three-group workloads plus high-hit overlapping groups. If most
candidates never reach this loop, or the complete effect is too small to
resolve, record that limited contribution and stop.

### 3. One using-field requirement without generic solver preparation

`IsUsingFieldsMatched` still prepares a matcher-pointer vector and a Hungarian
instance for one requirement. Reuse its existing `IsUsingFieldMatched` judge
in target order for that case. Preserve Get/Put/Any, null, cross-DEX and nested
behavior as currently implemented; do not combine a semantic correction with
this optimization.

Cost: low. First count eligible calls, row lengths and solver preparation.
Check mixed reads/writes, duplicate positions, a late unique witness, nested
reverse-field dependencies and the unchanged multi-requirement control.
The original one-left solver already stops at its first witness, so no
automatic reduction in judge calls is promised. Borrowed input and contiguous
scratch for multiple requirements are separate later experiments. Nested
queries must not overwrite one shared TLS scratch buffer.

### 4. Direct contiguous forward-field rows

Prefer `method_using_field_ids` to callers as the next storage experiment.
Forward rows are generated once in InitCache and then read by matchers,
getters and reverse construction. Callers additionally undergo cross-DEX
aggregation and mutation, making peak-overlap and ownership changes harder
to isolate.

Cost: medium. Use size_t offsets, checked row lengths and the existing
`(field_id, bool)` payload representation. Append directly while decoding;
do not first build vectors and flatten, and do not simultaneously pack payload
bit widths. Preserve publication, stable ranges, order, duplicates and the
existing identity/aggregation rules. On the measured arm64 layout, 24-byte
row headers becoming 12 logical index bytes provide about 29.75 MiB of header
opportunity; payload capacity may increase and this is not a peak forecast.

Compare QQ, QQ followed by reverse construction, sparse small rows, giant
rows and repeated mixed read/write output. Record growth, moved bytes,
overlap and complete lifecycle. Repeated peak or warm-query costs require a
conditional conclusion; fewer buffers alone are not success.

### 5. Omit the instruction walk when only reverse fields are missing

`DexItem::InitCache` sets `need_foreach_method` for RW, yet fills reverse rows
later from the saved forward-field rows. When forward data is already ready
and no opcode/string/invoke/field/number extraction is claimed, the earlier
instruction loop has no requested cache output.

Cost: low. Skip only that loop for the eligible case. Keep reverse filling,
cross-DEX aggregation and publication, and never renormalize already claimed
flags. Compare forward-then-reverse, full-first and QQ-tail with independent
ordered results. Measure instruction work, initial completion and total
lifecycle; source-level redundancy alone does not quantify production time.
This does not remove the completed reverse table and cannot explain or promise
to remove warm reverse-query regressions. End the experiment if the complete
effect is small.

## User proposal: sorted string-pool lookup

The user suggested binary search for StartWith in the sorted DEX string pool.
The wording about case matching could refer to either setting, so both are
covered here. Local source and format checks support the following boundaries.

### Ordering and existing semantics

The [AOSP DEX format](https://source.android.com/docs/core/runtime/dex-format)
requires string IDs to be ordered by UTF-16 content with no duplicate entries.
Its MUTF-8 representation does not always have the same bytewise order: U+0000
uses bytes C0 80. Even an ASCII needle cannot justify raw string_view ordering
over the whole pool. For example, the valid UTF-16 order U+0000, A, Z is not
the order of those MUTF-8 byte sequences.

With ignoreCase=false, a compatible prefix occupies one contiguous ID range
inside each DEX. Use a prefix-aware comparison in the DEX ordering to find
both bounds; do not invent an upper bound by appending an arbitrary maximum
character. Define C(s, p) to compare code units until the first mismatch,
return negative if s ends first, and return zero once p is exhausted. Then lo
is the first C >= 0 position and hi the first C > 0 position. Equal can use
lo with a final complete byte-equality check. Endpoints must represent S;
handle an empty pool and absent values. Absence makes only the corresponding
atomic condition false, without bypassing enclosing OR/NOT semantics.

With ignoreCase=true, folding the sorted pool does not preserve order: A, B,
a becomes a, b, a. One binary-searched interval over the original table is
therefore not a general solution. Enumerating case variants or constructing
a separately ordered index would be different experiments with their own
preparation and memory costs.

Current matching is byte-oriented, and GetIgnoreCaseChar folds only ASCII
A-Z. DEX data remains MUTF-8 while query strings can use standard UTF-8.
Normalizing both to Unicode would change some existing behavior, including
supplementary characters. The first range prototype should accept only
nonempty, case-sensitive ASCII queries without embedded NUL, correctly decode
pool content for ordering, and fall back for other cases. Decode unsigned
UTF-16 units, distinguish encoded NUL from the terminator, and do not combine
surrogate pairs for ordering. The bundled Utf8Cmp assumes valid MUTF-8; it is
not a general standard-UTF-8 query decoder or a bounded string_view comparator.

### Where the work moves

For each DEX and query, resolve an Equal ID or a StartWith [lo, hi) interval,
then walk the existing method_using_string_ids rows in their original order.
IDs remain local to their DEX. A range needs two integers, not enumeration of
every matching pool entry. This changes the per-reference predicate but does
not provide a string-to-method inverted index or remove all candidate and
reference traversal. Prepare once for each query, accessed DEX and matcher
semantics, with a shared completed plan across its work shards. Do not repeat
binary search per method, prepare every unvisited DEX eagerly, or reuse a bare
string ID across DEX files.

Let S be pool size, P eligible patterns, R visited references and C the cost
of one ordering comparison. Preparing ranges costs O(P log(S) * C). Checking
one requirement costs O(R), with possible earlier witnesses; a naive P-range
check per reference instead costs O(P * R). Direct prefix comparison also
stops at the prefix boundary, so its baseline cost is not a full-string scan.
AC scans text bytes and emits hits, sharing that scan across patterns. The
existing negative-string memo already skips proven empty scans in method
batches, so the current enabled memo belongs in the comparison.

Few Equal/StartWith patterns, many visited references and repeated references
to the same strings make ID predicates plausible. Sparse candidate filters
can make preparation dominate. If Contains/EndWith patterns still require AC,
adding ID checks may leave the text scan intact and add work. In such a split,
H3's empty-parse memo describes only the remaining trie and must not skip the
separate ID predicates. The existing composite prefilter also remains work.
Dense prefix matches need not enlarge the stored interval, but can still
increase accepted candidates, group processing and output. Pool coverage is
not method hit rate. Direct Equal may reject immediately on length, and a
dense prefix may give the direct loop an immediate witness. No speed percentage
follows from the asymptotic argument.

### Actual coverage and the next experiment

The existing groups.tsv contains 149 groups and 187 configured atoms. Applying
the adapter's SimilarRegex conversion gives 183 Contains and four Equal atoms,
with zero StartWith or EndWith atoms. There are 179 unique Contains strings
and four unique Equal strings. The three legacy groups also use Contains.
Consequently, the measured batch-stage shares are not prefix coverage and
cannot predict a prefix-range gain. SettingEntry's single exact literal is
the first real-workload three-way comparison.

Compare current nine-switch AC, direct exact/prefix comparison and per-DEX
ID/range lookup independently. Include preparation in complete API timing.
Use actual SettingEntry and complete QQ 1/11-pass runs, then controlled prefixes
with early/late/all-miss rows, sparse filters, repeated references, many
overlapping or dense prefixes, and mixed Contains groups. Check empty values,
ASCII case controls, NUL,
non-ASCII, isolated/paired surrogates, empty pools, longer-than-string patterns,
0x7f prefix endings and supplementary-character pool entries against the
unchanged predicate. Record plan comparison counts, decoded units, actual R,
ParseText bytes/hits and plan memory. Preserve enclosing logic, DEX isolation,
result order and duplicates. Do not select a runtime threshold before
measurements establish a crossover.

## Additional local findings and scope decisions

Two additional mechanisms were found during the local cross-check and then
confirmed in Pro's focused follow-up. They were not separate items in Pro's
original ranked five and have no measured gains:

* `AhoCorasickDoubleArrayTrie::StoreEmits` copies `output[currentState]` and
  `v[hit]` into local vectors before reading them. Profile that path and, if
  worthwhile, isolate borrowing these read-only arrays as its own experiment.
  The callback ParseText overload has similar copies, but its callback
  mutation/reentrancy behavior needs separate consideration; do not silently
  replace every copy across all overloads.
* Both bridge-level batch entry points initialize result-map keys with nested
  loops over the same group list. Source-level initialization is G by G even
  though the map is still empty of results. A single pass over group keys is
  a small independent candidate. Preserve duplicate keys and empty returned
  groups. Do not bundle it with containment and attribute their joint result
  to only one change.

Output has real copies: per-DEX batch Beans are copied into item containers,
bridge merging copies them again, and MethodMeta creates a temporary int32
parameter vector. Measure grouped output counts, pre/post-dedup counts and
copied parameter bytes before moving this ahead of the string work. Changes
to ownership, early deduplication or FlatBuffer Finish boundaries have broader
validation requirements than local copy elimination and must preserve the
first representative and every schema field.

Sparse searchIn enumeration and double packaged-task/future wrapping are
also source-backed opportunities, but current measurements do not identify
them as dominant. Scheduler experiments must preserve completion, exception
propagation, draining, join and worker TLS cleanup. Small proto/code-offset
side tables and another descriptor representation remain lower priority.

## First three bounded experiments

1. Attribute the three string stages above: one warm native stack profile and
   task/thread-local aggregate counts for string visits, ParseText/hits,
   group checks/intersection work, Beans and output counts. No per-candidate
   shared atomic increments or clocks, and no new monitoring framework.
2. Compare the narrow exact-string alternatives separately against the
   current combination: direct comparison and per-DEX sorted-pool ID lookup.
   Use the actual query and adversarial strings, then a separate prefix
   workload to test StartWith ranges without claiming QQ prefix coverage.
3. Independently compare only batch boolean containment against the same
   combination, retaining low/high-hit and overlapping-group controls.

If attribution rejects a hypothesis, implementation is optional and the
negative finding is a complete outcome. Use the existing finite paired-batch
and independent-confirmation method; include initialization, preparation,
result release and close. Diagnostic measurements are for attribution, while
final timing uses diagnostics/metrics OFF. Record workload-specific memory
and latency tradeoffs rather than demanding or claiming universal gains.
