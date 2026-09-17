# Uncached member descriptors: execution plan

The user authorized implementing and measuring removal of the long-lived native
method/field descriptor cache. The existing vector experiment is complete; its
artifacts and measurements stay immutable.

## Implementation boundary

- Add default-OFF `DEXKIT_EXPERIMENT_UNCACHED_DESCRIPTORS` and matching Gradle
  property. It requires structural cross-reference identities and raw descriptor
  input lookup, and excludes all other descriptor storage experiments.
- MethodBean and FieldBean own their descriptor strings in this experiment.
  Descriptor getters return owning strings. Class descriptors still borrow the
  immutable DEX image. No per-ID descriptor owners, ready arrays, cache locks,
  promotion thresholds, or vector borrowing protocol are present.
- Preserve descriptor bytes, FlatBuffer schema, managed APIs, output ordering,
  first representative selection, and textual cross-DEX result deduplication.
  Keep the current construction algorithm and raw lookup implementation fixed.
- This is a native C++ type/ABI change under the experiment. All native consumers
  must use matching compile definitions. A view into a returned string borrows
  that string, not the bridge. Do not return a view into a temporary.
- Short-lived Bean strings may be copied when existing result assembly copies a
  Bean. Those costs and simultaneous-result memory remain part of the experiment;
  do not hide them by changing unrelated result collection in the same candidate.

## Comparisons and acceptance

Use the same frozen Small14 optimization combination and toolchain for old dense,
current vector, old dense plus raw lookup, and uncached plus raw lookup. The raw
dense control separates lookup changes from removal of persistent caching.

Include sparse QQ with one and eleven passes, one/four query workers, full
method/field output once, twice and sixteen times, long descriptors, changed
working sets, repeated narrow/broad/prefix lookup, and concurrent callers.
Measure complete lifecycle, first/repeated stages, close, and process peak memory.
Record all valid samples in balanced AB/BA pairs with an independently seeded
confirmation. Freeze the exact finite matrix before formal timing; no builds,
correctness checks or profiling run during that batch.

Verify full independent result oracles, duplicate definitions, invalid lookups,
SSO/heap owning strings, copy/move/vector relocation, nested metadata, worker and
concurrent API results, and results retained after bridge destruction. Check
diagnostics for repeated generation and zero persistent descriptor storage.
Run sanitizers, required JVM/Core tasks, Android four-ABI release build, and docs.
Review the actual source delta through the existing authorized Pro conversation.

## Progress

- [x] Implement and inspect the guarded candidate and owning result contract.
- [ ] Complete native/oracle/lifetime/sanitizer/JVM/Android verification.
- [ ] Read and reconcile the complete source review.
- [ ] Freeze and execute the paired performance matrix.
- [ ] Archive all evidence, report benefits and adverse cases, and push the fork.
