# Direct slot-array publication: source review

The complete 6m07s Pro answer was read in the
[existing conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3).
It reviewed `eed75a8 -> 33b1224`: the complete cache header, existing hybrid
component checks and execution plan, plus descriptor getter/Cold and numeric
and text Bean consumers. It also checked the previous result report and its
post-batch mechanism analysis. It did not inspect the binaries, rerun tests or
recompute the raw paired statistics.

No confirmed new correctness blocker was identified. Local source and executed
checks agree with the following conclusions:

- Publishing `dense->slots.get()` preserves ownership: moving the unique
  DenseIndex owner does not move the array, and clearing the pointer hash does
  not destroy the array or deque strings. The array is never resized, replaced
  or reclaimed before quiescent close.
- Valid insertion precedes conversion. For `index = 32*q + shard` and
  `index < N`, `N > shard` and `q < 1 + (N - 1 - shard) / 32`. A published
  array therefore has at least one slot and covers the validated index,
  including a final partial shard. Checked allocation arithmetic is unchanged.
- Array construction, initialization and existing-value stores precede its
  release publication. Readers still acquire the array pointer and the target
  atomic slot; later strings still use a slot release. Removing an ordinary
  owner-to-slots load does not remove either publication relationship.
- The slow path rechecks the published array after acquiring the mutex, so a
  reader delayed across conversion does not query the cleared hash. The same
  recheck preserves unique construction of a dense empty slot. A pointer to an
  empty string remains a hit.
- NormalDomain mirrors the new pointer type. Owning allocations, DenseBytes,
  conversion policy and counters are unchanged. Sparse-only still skips
  publication and conversion; FAST_HITS disabled uses the same generic probe.

Existing bounds, retained-view, concurrent growth, dense-empty-slot and forced
stale-sparse checks cover this replacement without a new testing interface.
The latch interleaving is diagnostic-only. For this source version, all 184
native driver commands, 42 normal smokes, four QQ full oracles, 71 JVM tests
and four Android ABI builds completed before formal timing. Five ABI layouts
and 16 paired diagnostic cases also reconcile. These are local execution
results; Pro received their progress state at submission, not a rerun.

The reporting limits remain: a removed load is not an inferred speedup or an
allocation saving. Normal executable assembly provides the mechanism evidence;
the fixed public API comparisons determine its practical value. Same requested
layouts do not prove identical normal physical peaks. Diagnostic dense_hits
does not describe all work inside the public API, and conversion timing and
overlap remain partial measurements.

QQ and unconverted low-density domains cannot directly benefit from the removed
load. Their measurements guard cold/sparse costs and must not be attributed to
dense reads. Report the within-batch before/after change and direct old-dense
residual separately. No additional matrix or production correction was needed
for this review; the pending finite comparison decides the retained scope.
