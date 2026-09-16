# Memory layout follow-up

Starting source: `174b69ad699702ee6e38da0b998192fca470d030`; native caller
implementation: `e3ec3622ff9bc3b3613f934356bbd1cf6b4408c5`.

The user authorized continued implementation, Pro collaboration and measurements
to distinguish useful memory changes from regressions. The first new design
question is whether a growing hash table can replace the mostly empty descriptor
slot arrays. Keep work in the existing experimental branch and default all new
options OFF. Public API/schema, scheduler and exception recovery are outside scope.

- [x] Add a small diagnostic census after queries drain: descriptor page
      occupancy, actual name-hash allocations, cross-reference occupancy and
      separate class method/field rows.
- [x] Obtain and locally check Pro's descriptor-hash design advice against
      fixed source, borrowed-view lifetime and the bundled hash implementation.
- [ ] Independently implement and validate 8-byte cross-reference storage.
- [ ] Independently evaluate a growing descriptor hash cache, preserving stable
      standard-string objects and synchronized map access while entries grow.
- [ ] Independently evaluate 4-byte field-use tokens while keeping row vectors.
- [ ] Review stable source increments with Pro, complete native/JVM/Android
      checks, then freeze finite paired measurement plans before timing.
- [ ] Report retained capacity, temporary overlap, physical peak and complete
      lifecycle, preserving adverse cases and independent confirmation.
- [ ] Use those results and the completed census to rank remaining candidates;
      record rejected/conditional options rather than repeatedly redesigning
      an unsuccessful cache without new evidence.

## Comparison boundary

Use Small13 plus COMPACT_CALLERS as the fixed 14-option experimental baseline.
The preceding caller report records its own tradeoffs; this does not silently
enable it in default library builds. CANDIDATE_PIPELINE/SLICES, POINTER_DESCRIPTORS,
PAGED_DESCRIPTORS and COMPACT_FIELDS remain OFF unless explicitly being compared.
Measure each new option independently first. Do not add percentages from old
baselines or substitute a smaller payload for a measured process-peak benefit.

Normal timing builds disable metrics and diagnostics. Builds and correctness
checks must not overlap formal timings. Reuse frozen complete ordered fixtures,
QQ verification, size boundary checks and host sanitizer coverage; build the
Android AAR for all four ABIs. Add tests only for newly changed contracts.

## Descriptor constraints

Cache only generated values; do not evict published strings. Every returned
string_view, including a view into an SSO string object, remains valid until
DexItem destruction. A flat hash table holding strings directly does not meet
that contract when it relocates values. Stable nodes or stable object storage
must separate string lifetime from hash-table bucket growth.

All accesses to a growing table require appropriate synchronization. Independent
shard locks must guard independent tables, not unrelated buckets of one table
that can rehash globally. Get the returned view while the owning table/storage
metadata is protected; only immutable character storage is read after unlock.
Avoid recursive descriptor construction under the same lock and preserve the
existing formatting/copy-capacity policy for the first attributable prototype.

Keep sparse QQ, scattered IDs, full-coverage SSO, long-string output, hot lookup,
concurrent cold inserts/rehashes and 1/4-thread cases. Measure creation, first
API, warm APIs, result release, close and physical peak. Node/bucket/character
allocations and unused slots/pages must be accounted separately.

## Remaining candidates

Class-field implicit ranges, string offset/length views, method-code offsets,
raw proto lookup and exact forward invocation capacity remain candidates.
Type-name/source metadata changes, fixed pages of real string objects, and
less frequently built annotation/field/opcode/number caches need evidence of
their cost or occupancy before their scope is chosen. Previously regressing
pointer/paged-byte/source/compact-field prototypes are historical evidence,
not untested opportunities or default recommendations.
