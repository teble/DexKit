# Sparse/dense descriptor cache experiment

The user proposed a cache that starts sparse, converts to a vector at a threshold
and releases the sparse table. This continues the authorized memory optimization
work. The preceding 76-sweep memory-layout comparison remains frozen at its own
source and artifacts; this experiment starts separately after it finishes.

## Chosen design and review

Pro's complete design answer was read in the
[existing conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3).
It inspected the fixed `05e5a41` node cache, descriptor getters/Cold and text
lookup overloads, plus the bundled phmap extraction, node-handle transfer,
clear and rehash implementations. It did not inspect a hybrid implementation
or recompute the supplied preliminary timings. Local source checks agree with
the relevant ownership and release mechanisms.

Use one new payload model: lazily construct a `std::deque<std::string>` for each
used method/field domain in each of the existing 32 shards. Only append final
strings using the existing temporary then lvalue-copy construction policy.
Never erase, replace or move published string objects. Keep every deque metadata
operation under the shard mutex. Hash/dense entries retain pointers, not deque
iterators. The [C++ draft deque contract](https://eel.is/c++draft/deque.modifiers)
preserves element references during insertion at an end; this was checked
directly in addition to Pro's citation.

The sparse index is a flat hash map from local ID to final string pointer.
The dense index is a fixed-size array of atomic string pointers. Conversion is
one-way, independently for each DEX, method/field domain and shard. A domain with
N total IDs gives shard s zero slots when N <= s, otherwise
`1 + (N - 1 - s) / 32` slots; valid ID maps to shard `id % 32`, slot `id / 32`.
Check the logical bound and allocation multiplication before access/allocation.

## Publication and reclamation

Dense hits acquire the published array pointer, then acquire the slot pointer;
they neither hash nor lock nor access mutable deque metadata. Sparse/cold paths
take the existing shard mutex and recheck the published state before reading a
map or constructing a missing string. Null slots mean missing, while a pointer
to an empty string is a valid hit.

Under the mutex, initialize the entire dense array to null, copy every existing
stable pointer into its slot, install the owner, clear the pointer-only sparse
map and call `rehash(0)` to release its buckets. Publish the complete dense
index with release. Later cold insertions publish their individual slot with
release after final string construction. No array resizing or backward switch
is permitted. The existing quiescent-close requirement remains in force.

Copying strings from the current node cache into a new deque after views have
escaped is not this design. The new payload owner must be selected from cache
creation. Node-handle extraction would preserve old addresses, but it retains
per-node/key/deletion costs and adds handle ownership; it is not the selected
prototype. No private phmap ownership APIs or custom block allocator are needed.

## Fixed conversion rule

For the first prototype, promote on a new insertion/capacity change when the
requested dense array plus index-owner bytes are no greater than the current
sparse bucket/control/alignment bytes. Use target-ABI sizes and the bundled
table's actual layout. Do not fit a density percentage to QQ or count hits in
the normal hot path. Keep payload storage common between sparse and dense so
its bytes cancel in this local index comparison.

Conversion still overlaps the old hash and new dense allocation, initializes
the array and scans hash capacity under the lock. A triggering hash growth may
already have paid for another old/new overlap. Count those costs and concurrent
conversions separately from steady retained bytes. Deque block/directory slack
and allocator rounding need diagnostic allocation accounting and physical peaks.

Low-density hot keys may never convert. A workload that ends immediately after
conversion cannot amortize its pause. Neither is fixed by silently tuning a new
threshold or adding access-frequency adaptation to this experiment.

## Work and evidence

- [x] Read and locally check the focused Pro design and primary deque contract.
- [x] Finish and preserve the preceding fixed memory-layout evidence.
- [x] Implement the lazy-deque sparse-only and single-rule hybrid variants,
      default OFF, preserving current formatting, local IDs and lookup semantics.
- [x] Validate threshold-before/at/after, N=0/1/31/32/33 and nonmultiples,
      unequal member domains, last IDs, actual stale-sparse-reader interleaving,
      concurrent construction/conversion and retained SSO/long/empty views.
- [x] Review the stable source increment, run host sanitizer/JVM and four-ABI
      Android builds, and reconcile real payload/index allocations.
- [ ] Freeze a finite comparison with old dense, current node, new sparse-only
      and new hybrid builds; hold all other Small14 settings fixed.
- [ ] Keep QQ sparse, full SSO 2/16 repeats, long output, wide/prefix/hot lookup,
      one/four real calling threads, and before/at/after conversion followed by
      immediate close. Report first, warm, close, lifecycle and physical peak.
- [ ] Use one main phase and one confirmation to retain, conditionally retain
      or stop the route; preserve all adverse cases and unchanged result oracles.

The sparse-only comparison is necessary to separate payload/flat-index changes
from conversion and the extra state check. This document is a design and work
plan, not evidence that the unimplemented hybrid already improves performance.

## Prototype progress

`DEXKIT_EXPERIMENT_SPARSE_DESCRIPTORS` selects the common lazy-deque payload
with a flat pointer index. `DEXKIT_EXPERIMENT_HYBRID_DESCRIPTORS` also permits
conversion. Both default OFF and are mutually exclusive with node/pointer/paged
storage. Desktop and Android Gradle properties select the same Core options.

The standalone component passed normal, diagnostic, and ASan/UBSan host runs
(leak detection disabled). It covered real simultaneous construction/conversion,
borrowed SSO/long/empty characters, deque block/directory growth, N=0/1/31/32/33
and 1057 with unequal method/field domains, and a latch-controlled stale sparse
reader. A fresh cache was stopped before, at and after its first conversion;
the subsequent same-slot concurrent cold insertion constructed one empty string.
Full native/JVM/Android and API oracle validation subsequently passed as listed below.

The measured host layout has 6680 fixed cache bytes. For 60000 total method IDs,
shard 0 has 1875 dense slots. At 448 records its hash requests 8696 bytes; the
449th insertion grows the hash to 17400 bytes and triggers the 15008-byte dense
index. The diagnostic table then reports zero hash capacity and bucket bytes.
These are requested layout bytes, not allocator-rounded or process peak bytes.
The workload accepts an explicit shard member count, allowing 448/449/450 members
followed by immediate close and a separate one-member repeated workload without
embedding that observed threshold in production code.

## Completed validation and fixed comparison plan

The source review and the adopted diagnostic limits are recorded in
[HYBRID-DESCRIPTORS-REVIEW.md](HYBRID-DESCRIPTORS-REVIEW.md).
The engine commit for all new artifacts is `bf9cee531da6a5d4f237398350eed05d7fcf96c5`.

- Normal, diagnostic, independent-storage and full-Core ASan/UBSan validation
  completed 59, 65, 32 and 42 driver commands respectively. The expected aborts
  reject out-of-range method/field IDs. All result oracles agree, including
  symbol/overload text and bytes, cross-DEX caller/invoke/field results, and the
  small/short/long/unresolved field fixtures. Sanitizer leak detection was OFF.
- All four normal variants passed 56 native workload smoke runs. The public
  diagnostic API confirmed no conversion at 448 members per domain, one per
  domain at 449, and no further conversion at 450; the converted tables retain
  zero sparse bucket capacity. The one-member case means two total descriptors.
- All four normal variants matched the frozen QQ baseline at p11/w4 and p1/w1;
  both diagnostic variants also matched p11/w4. The hybrid late field reader /
  writer output matched its independent frozen oracle. These are 11 QQ checks.
- Required Gradle Core/JAR/JVM/AAR tasks completed: 71 tests, zero skipped, and
  arm64-v8a, armeabi-v7a, x86 and x86_64 libraries in the release AAR. Android
  execution/performance was not measured. The JAR stayed at SHA256
  `aad51ff2f604056be1a1b2cbb0a41adae32ac17e855e7eac8a6db10898383e9a`.
- Five target layout probes matched actual normal Cache and deque-owner sizes
  against the diagnostic normal-layout calculation. See the review table.

Two development-driver errors were corrected before measurement: normal builds
initially requested diagnostic-only check targets and CMake rejected them; a
census assertion initially counted create/workload-complete/pre-close snapshots
as one phase. Failed configure logs are retained. The phase-specific census was
rechecked and build/validation drivers completed after correction; these did not
require a production-source change or removal of any performance sample.

The finite plan contains 108 sweeps / 1296 fresh processes: six balanced pairs
per case in the main phase and six in one independently seeded confirmation.
Three comparisons cover the four builds: old dense to hybrid (net effect), node
to new sparse-only (payload/flat-index effect), and sparse-only to hybrid
(conversion and additional state check). Other Small14 flags remain fixed;
packed cross identities and packed field uses are OFF in this experiment.

There are 15 native cases and QQ p1/w4, p11/w4, p1/w1 for each comparison and
phase. Native cases preserve full SSO r2/r16, three sparse distributions, long
output, wide/prefix/hot lookup and one/four real calling threads. Added boundary
cases select 448/449/450 methods and the same number of fields, use one output
pass, then close immediately. The low-density hot case repeats one member per
domain 100000 times. No threshold changes are allowed between phases. Timing
starts only after builds, correctness checks and source review finish.
