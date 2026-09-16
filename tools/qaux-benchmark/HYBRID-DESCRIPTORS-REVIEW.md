# Hybrid descriptor source review

The complete final response was read in the
[existing Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3).
The source range was `5c312bf592afca6cc6338388c70f50b62e0026ad` to
`bf9cee531da6a5d4f237398350eed05d7fcf96c5` in `teble/DexKit`.
Pro used the selected GitHub connector. An initial lookup for
`tools/qaux-benchmark/descriptor_checks.cpp` returned 404; it then located and
read the actual `Core/dexkit/descriptor_checks.cpp`. That initial error did not
leave a final source gap.

The review covered the complete new cache and component, descriptor workload,
sweep and execution document; relevant DexItem initialization/getters/member
declarations, diagnostics, descriptor checks, CMake and both Gradle files;
and the bundled phmap flat-table allocation and release implementation.
Pro did not run builds, tests or timings, or inspect subsequent raw measurements.
Its response treated the then-pending local validation as pending.

## Source conclusions and local reconciliation

No concrete new production ownership, bounds, duplicate-construction or
publication bug was found. Local inspection agrees: the final strings are
appended before their first publication and never relocated; hash/dense indexes
hold pointers only. The slow path rechecks the dense publication while holding
the shard mutex. Initial slots are published by the outer release, later cold
slots by their own release; both reads acquire. Clearing the hash does not own
or destroy the strings. Quiescent close remains required.

The fixed byte rule is evaluated only on capacity changes. This is sufficient
because the flat map has zero external per-element bytes and the dense slot
count is fixed. The bundled layout helper is O(1) for this map. Empty-table
`rehash(0)` releases the sparse allocation. Independent storage macros exclude
the old optional arrays, ready arrays and separate locks in both build paths.

The component really forces the stale-sparse-reader interleaving and concurrent
first fill of a dense null slot. Those hooks and conversion-count assertions
exist only in diagnostic builds. Normal component checks exercise the same
production algorithm, retained characters and concurrent calls, but do not run
diagnostic hooks. The distributed growth case also grows/converts the shards
holding the retained long and empty strings.

The review's remaining observations are reporting limits, not reasons to alter
the production cache or add an OOM recovery framework:

- Allocator counters describe the diagnostic deque's actual block/directory
  requests. Subtracting the diagnostic allocator's owner-object size difference
  does not independently prove an identical allocation trajectory for every
  target standard library's normal allocator. Report these as diagnostic
  requested capacities, not an exact division of normal physical memory.
- Cache-level `instrumentation_bytes` already includes the sum of table-level
  `payload_instrumentation_bytes`. The latter is detail, not another addition.
- `promotion_overlap_bytes` is the live final payload plus old hash plus new
  dense allocation for one domain. It omits the still-live triggering temporary
  descriptor, fixed cache and allocator overhead. It is neither the full
  transition workspace nor a process peak.
- `promotion_ns` covers the instrumented conversion function only. It excludes
  lock waiting, Cold construction, deque append and the triggering hash growth.
  The normal first API/lifecycle measurements include those costs. Hash growth
  overlap and conversion overlap cannot simply be added into a physical peak.
- `--selected-count=449` selects 449 methods and 449 fields, producing two
  domain conversions on the measured host. A count of one selects one member
  per domain, two descriptors in total. Method/field result buffers overlap as
  in the existing workload. Each domain's actual conversion is checked in the
  public API diagnostic output, not inferred solely from the component probe.

## ABI evidence obtained locally

After the review request, normal and diagnostic layouts were compiled for all
five targets. The reported normal cache size matches the real normal class on
every target, including 32-bit padding. The two template variants have identical
fixed layout. The owner difference is reported separately for every used domain.

| Target | Normal cache bytes | Diagnostic cache bytes | Normal deque owner | Diagnostic deque owner |
| --- | ---: | ---: | ---: | ---: |
| macOS arm64 | 6680 | 18472 | 48 | 64 |
| Android arm64-v8a | 5912 | 17704 | 48 | 64 |
| Android armeabi-v7a | 2444 | 10008 | 24 | 32 |
| Android x86 | 2444 | 10008 | 24 | 32 |
| Android x86_64 | 5912 | 17704 | 48 | 64 |

These compile-time checks establish object layouts, not Android runtime
allocation or performance. JVM tests and four-ABI AAR packaging are recorded
separately by the execution evidence. New performance conclusions require the
fixed normal-build main phase and its independent confirmation.
