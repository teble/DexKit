# String inverse index source review

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

The first review read `9432879..3ccb765` through the already selected GitHub
connector, including the reverse index, ordinary and batch find paths, matcher
cache/TLS integration, initialization/admission/cross-reference context, build
options, and existing query checks. The complete reply arrived after 14m39s.
It did not execute tests or measure performance.

The review found no concrete false-negative or cross-DEX TLS counterexample in
the supported input paths. It confirmed class projection before keyword AND,
the root undefined-owner exclusion, rank boundary handling, checked ID widths,
and call_once publication. The execution agent verified these against source.

One confirmed budget defect was fixed: construction used method-sized planes,
while consumption added a type-sized union that could be larger. The example
with 4096 methods, 60000 types, one keyword and 32765 groups exceeded the stated
16 MiB budget by 6480 bytes. A shared checked helper now covers both phases;
boundary and overflow-shape checks accompany it. Range-only root candidates
also check their bitmap request. Diagnostics and documentation identify these
as per-DEX requested bitmap bytes, not a process or allocator limit.

The new integration checker covers root-plus-NOT/AND, shared-vector identity,
and a remote method with the same numeric ID but opposite string truth value.
It also starts ordinary Contains and Batch on fresh bridges after only forward
prewarming, verifies the inverse index was absent, and checks once-per-DEX
construction and complete result bytes. Four diagnostic configurations pass.

The comment about direct/virtual encoded order was corrected: InitBaseCache
sorts class_method_ids, and Batch preserves the existing sorted row order.
Ordinary classes separately preserve ClassDefs order, including the reversed
fixture. Allocation exceptions are not caught as a general fallback; no OOM
fault-injection claim is made.

The review also identified a performance boundary: ordinary fallback still
uses one task per DEX after the initial routing decision, and successful broad
queries can serialize expensive remaining predicates. An additional one-DEX
nested workload measures that tradeoff. Gains include candidate traversal,
less repeated matching, scheduling changes, and removal of the Batch negative
memo on the new path; they cannot be attributed solely to fewer AC scans.

The final incremental review read `3ccb765..cc9f893` and completed after 6m52s.
It confirmed the budget arithmetic/call sites and the actual new TLS and cold
concurrent paths, with no new source blocker. Of the eight integration queries,
one is a root-only control and seven have child predicates. The start latch
establishes concurrent cold entry, not a forced simultaneous pause inside
call_once; the once-per-DEX log checks are limited to their marked windows.

The remaining nonblocking recommendation was adopted after measurement:
`string_nested_shape_checks.cpp` reuses the exact measured query builder and
links the unchanged Core archives. Across control, candidate, sanitizer and
standalone variants, every one of sixteen positive calls returns the ordered
4500 IDs/descriptors and every negative call returns zero. All 24 timed broad
samples also equal the independently calculated aggregate identity. No timed
engine or workload binary was replaced, and no additional engine change or
review round was needed.
