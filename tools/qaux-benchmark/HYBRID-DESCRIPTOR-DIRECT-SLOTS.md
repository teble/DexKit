# Hybrid descriptor direct-slot publication experiment

The completed [fast-entry batch](HYBRID-DESCRIPTOR-HOT-HITS-RESULTS.md) did not
establish a general speed benefit. In the actual timed text-lookup functions,
the compiler already inlines the descriptor probe. Its dependent reads remain:
published DenseIndex, the owner's slots pointer, an atomic string pointer, then
the string length/body. Post-batch diagnostic checks found zero new construction
or conversion across 4186623 additional warm calls, all lock-free dense hits.
Normal statistical samples locate most hybrid time in that inlined lookup loop;
they do not assign a causal percentage to a particular load.

The next single change publishes the fixed atomic slot-array address directly.
Keep the existing DenseIndex owner, its allocation, the slot allocation, deque
payload, shard layout, byte-cost rule and fast/Cold entry split. This isolates
one owner-to-slots dependent read. Do not remove the owner, relocate the hot
arrays, lower memory ordering or retune conversion in this comparison.

Under the mutex, initialize and fill the same array, save its address, install
the same owner, clear and release sparse buckets, then release-publish the array.
Readers acquire that pointer and acquire their slot. Later cold inserts retain
the same slot release; slow readers still recheck under the mutex. A domain can
convert only after a valid insertion, so the published array has at least one
slot. It is never resized or replaced, and close still requires quiescence.

- [x] Implement direct array publication without changing owning allocations.
- [x] Run existing component/public API oracles in normal, diagnostic, isolated
      FAST_HITS-OFF and sanitizer builds; reconcile bounds, counters, requested
      capacities, conversions and borrowed views with the fixed previous build.
- [x] Verify actual timed getter/call-site assembly loses the intended load,
      check object layouts on the supported ABIs and run required JVM/AAR checks.
- [ ] Review the fixed source increment with Pro, including the prior complete
      result and actual call-site/profile evidence.
- [ ] Freeze one finite before/after comparison, with old-dense references for
      the remaining dense residual and QQ memory benefit. Include sparse/cold,
      full SSO, long output, wide/prefix/hot, concurrency and conversion edges.
- [ ] Complete a main phase and one confirmation, retain adverse samples and
      decide whether this one dependent-load change has demonstrated value.

The baseline is the immutable `56c680c` hybrid artifact. This new experiment
has its own evidence directory and does not rebuild or change the completed
52-sweep batch. No new public API or default feature enablement is proposed.

Validation completed before timing: 184 native driver commands, 42 three-way
workload smokes, four QQ full-oracle runs, 71 JVM tests with zero skips, and
four Android ABI builds. Sixteen paired diagnostic cases reconcile all 4352
tables, retained requested layouts and completed-access counters. Five ABI
normal/diagnostic object layouts match the preceding implementation. Host
ASan/UBSan disables leak detection; Android execution is not covered.

The actual normal dylib getters change 27 -> 26 instructions and keep both
acquire loads. In the timed executable, method/field text lookup changes
77 -> 76 / 67 -> 66 instructions, respectively, by removing the ordinary
owner-to-slots load. The method lookup frame remains 160 bytes. These are
machine-code observations, not measured cycle or lifecycle improvements.

Freeze 52 sweeps / 624 fresh processes: 18 before/after cases and eight direct
old-dense/after residual references, each in a main phase and one independently
seeded confirmation, with six balanced AB/BA pairs per sweep. Before uses
`56c680c`, after uses `33b1224`, and old dense uses `bf9cee5` with hybrid disabled.
Keep the previous SSO2/16, sparse distributions, long output, wide/prefix/hot,
one/four calling-thread, 448/449/450-per-domain immediate-close and low-density
hotspot cases, plus QQ p1/w4, p11/w4 and p1/w1. Preserve every successful sample;
do not rebuild, run correctness checks or profile during the timed batch.

This plan is a historical input once timing is frozen. Record the completed
Pro review and measurement verdict in sibling review/results documents instead
of changing the frozen plan between phases.
