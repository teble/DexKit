# Compact caller experiment

Baseline: `6fc9595137773c20d8fc98c44f37c16146bb48f4`, using the existing
Small profile plus SKIP_EMPTY_CANDIDATES and MOVE_FIND_RESULTS. Keep candidate
pipeline/slicing OFF. This experiment changes caller storage and construction
only; cross_info and field storage remain unchanged.

- [x] Implement a default-OFF COMPACT_CALLERS option on both build paths.
- [x] Validate exact ordered results, raw reference rows, late construction,
      shared-query publication, stable spans, and size arithmetic.
- [x] Review the fixed implementation with Pro and resolve concrete issues.
- [x] Complete native/JAR/JVM and Android ABI checks before formal timing.
- [x] Measure QQ and bounded adverse workloads in balanced fresh processes,
      then repeat the same cases in an independent confirmation.
- [x] Record retained bytes, temporary bytes, physical peak, lifecycle and
      cold/warm API results, including regressions and unresolved intervals.

Completed results and tradeoffs: [COMPACT-CALLERS-RESULTS.md](COMPACT-CALLERS-RESULTS.md).

## Construction contract

Each DEX owns one size_t counter per method ID while callers are being built.
Cold joint invocation/caller extraction counts during the existing instruction
walk. Late caller construction counts the already published invocation rows.
Identity resolution and its existing cursor/representative behavior stay intact.

Allocate one exact final edge array per DEX and an M+1 size_t prefix directory.
The source rows transferred by cross-DEX aggregation remain empty. Each target
row contains its original local contribution first, followed by source DEX and
pending-work order. Duplicate instructions remain duplicate edges.

Reuse the count array as the fill cursor array. Each pending import retains its
length until validation; this is additional temporary storage and must be counted.
Assign disjoint destination segments before parallel fill, so completion order
does not choose edge order. Reuse the existing warmup/publication barrier. Release
all count/cursor and pending-import storage before publishing final callers.
Debug and DIAGNOSTICS builds check the assigned segment endpoints and perform
the final fill-validation replay. Normal release omits that replay; checked
arithmetic and whole-array write bounds remain active. This follows the review
correction rather than treating disabled DEXKIT_CHECK expressions as validation.
Later RW/full warmup must not relocate published caller spans.

Use a trivial two-field edge record with the same logical u16/u32 values and
8-byte layout on the current host. Its exact array can be allocated without
zeroing every edge before the fill. The existing Hungarian matcher may borrow
the immutable span, as other compact relations already do; report that effect
as part of the representation change. No scheduler or exception recovery work
belongs to this experiment.

## Validation and measurement boundaries

Use complete ordered FlatBuffer results on bounded fixtures and independent
raw relation expectations. Cover local/cross-DEX repeated edges, unresolved
references before resolvable members, duplicate definitions, empty source rows,
zero rows, cold/joint/late/full initialization, 1/4 workers and retained views.
Keep long rows, empty rows and many sources importing into one target as adverse
cases. Check the option independently of compact forward invocation storage.

Normal measurements disable metrics and diagnostics and use identical compiler
settings. Do not compile while timing. Use six balanced pairs per case and one
independent confirmation, retaining all accepted samples. Include QQ 1/11 passes
and native cold/late caller construction, early/late/multiple matches and output.
Freeze the exact finite measurement list after fixture validation and before
the first timed run. Do not select cases afterward to hide regressions.

Capacity accounting is separate from process physical memory. Report raw DEX,
final directory and edge sizes, real edge counts/capacity, count/cursor storage,
pending-import capacity and release state. A smaller final table does not by
itself prove a lower build peak or faster complete lifecycle.

Development validation: the existing relation checks and tiny/mixed/giant
invocation checks pass, including the previously frozen 29-query outputs.
Both new control/compact diagnostic builds pass the caller index component
checks and the five-DEX fixture (33 methods, 31 ordered edges), whose authored
raw-row oracle is independent of native output. Their full dumps match.
The 1/4-worker cold/late/full and queued-query cases check source-row clearing,
released temporary capacity and stable caller addresses through later RW/full.
The final `e3ec362` passed all 66 native commands again, 71 JVM tests, Android
AAR construction for all four ABIs, six QQ verification runs, and 12 additional
caller/invocation checks on mostly-empty and one-source fixtures. The final
fixture includes two successful zero-count cross-DEX bindings. Ordered output
and raw rows match the controls, including ASan/UBSan runs.

The extended invocation generator adds an optional source count; its default
fixture APK remains byte-identical. A nine-DEX case with 5,000 methods per
source keeps most reverse rows empty while eight sources share a target.
The optional one-active-source setting also covers one source contributing
131,072 repeated calls; defaults retain the original fixture bytes.

## Frozen formal cases

Two sequential phases (main and independent confirmation), each with six
balanced fresh-process pairs per row. Native rows use 16 API repetitions;
QQ rows use the listed passes. Total: 34 sweeps, 408 fresh processes. Seeds,
commands and input hashes are frozen before the first measurement in
`evidence/compact-callers/v2/raw-evidence.tar.gz:measurements/frozen-plan.json`.

| Fixture | Modes |
| --- | --- |
| tiny | caller-match-cold-w1; caller-output-late-w4 |
| mixed | caller-match-cold-w4; caller-match-late-w4; caller-multiple-late-w4; caller-output-full-w4 |
| giant | caller-early-cold-w4; caller-match-cold-w4; caller-multiple-late-w1; caller-output-cold-w4 |
| mostly-empty | caller-match-cold-w1; caller-match-late-w4 |
| one-source | caller-match-cold-w1; caller-match-cold-w4 |
| QQ all | 1 pass / 4 workers; 11 passes / 4 workers; 1 pass / 1 worker |

Keep every successful sample, including outliers. No native build, Gradle build
or functional validation overlaps formal timing. The QQ launcher compiles its
Java adapter serially before each measured JVM; this is outside the measured
lifecycle. OS file caches are not flushed.

Staged workload labels have limited scopes. Cold relation preparation includes
forward invocations, caller identity and aggregation; full includes all caches.
Late preparation separates forward setup from caller completion. Both are
already inside setup/lifecycle and must not be summed again. The warm memory
snapshot is after index preparation and before timed queries. The closed
snapshot is after bridge destruction while the small query/metadata inputs
remain alive. Memory probe calls are included in complete lifecycle time.
