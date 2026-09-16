# Hybrid descriptor fast-entry source review

The complete 7m23s Pro answer was read in the
[existing conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3).
It inspected `e454b24 -> 56c680c`, including the cache header, component checks,
execution plan, DexItem declarations, both getters/Cold entries and their text
lookup callers. It also read the previous hybrid result report. It did not
download raw samples, rerun tests or disassemble the new artifact; the machine
code observation came from the local execution record.

No confirmed new correctness blocker was identified. Local checks agree: both
probes validate bounds and use the existing acquire publications; a miss makes
no state commitment and the slow path still rechecks under its mutex. Stable
dense indexes and string objects are never replaced. The new Cold entry does
not repeat the outer Called counter. FAST_HITS disabled, sparse-only and the
other storage branches preserve their intended paths.

The following reporting limits are adopted:

- `calls` counts completed cache access paths, not every probe. A failed outer
  probe counts nothing. A later generic hit counts one call/hit; a generic miss
  counts one call before the slow path, which adds a hit only if its recheck
  finds an existing value. Quiescent `calls - hits == records` still holds.
- `dense_hits` means successful lock-free dense probes. It excludes dense hits
  found under the mutex and includes generic hits after a Cold factory has
  already been materialized. It does not measure factories avoided. Unchanged
  calls/records/capacities cannot show that the extra failed probe is free.
- The component GetWithProbe helper receives an already-created Build argument.
  It verifies semantics, counters and concurrent publication, not factory
  elimination. That mechanism is evidenced by actual normal DexItem assembly
  and its effect is evaluated through unchanged public API workloads.
- The fill between an outer miss and generic hit is arranged sequentially in
  CheckTryGet. The separate diagnostic latch test forces the real interleaving
  after the generic miss and before the slow mutex, with conversion and hash
  release by another thread. Neither should be described as a forced pause
  between the two DexItem functions.

The previous conditional storage verdict remains appropriate. QQ and the
one-member-per-domain workload do not convert; they cannot directly gain from
an outer dense hit and remain guards against added cold/sparse cost. The 52
fixed sweeps are sufficient; separate before/after entry improvements from the
old-dense residual, and do not multiply percentages from different batches.

If a large dense residual remains after this batch, the next bounded question
is the dependent load chain: published DenseIndex, its slots pointer, the slot's
string pointer, then string length/body. Wide cached-text lookup repeats it for
each candidate, even for comparisons that quickly fail by length. Its existence
is a source fact; its contribution remains a hypothesis. A subsequent focused
getter/call-site sample should establish that warm construction and conversion
are zero before proposing layout changes. This review does not authorize or
claim a measured win for such a change.
