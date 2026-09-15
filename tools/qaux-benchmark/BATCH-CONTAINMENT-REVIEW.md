# Batch containment review

The authorized [Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)
completed a source review of fixed `a997da6eacb879cf9c4407adfe24073f19d73d01`
after 9m 38s. It read the two batch entry points, diagnostic header, CMake and
both Gradle files; the batch execution plan, query/check/oracle/fixture/workload
tools; and build_native/workload_sweep. It also read bridge preprocessing and
merging, ordinary keyword construction, and the completed Prefix report/review.
It did not rerun tests or recompute the paired samples. No new correctness or
lifetime blocker was found in the two includes replacements.

## Confirmed boundaries

Both sets use the same default string-view comparator. Complete containment is
equivalent to the original intersection-size condition, with the correct
argument direction. This is a comparison of the already produced keyword sets,
not DEX string-pool binary search. No shared mutable state, range ownership or
publication protocol is added. The same empty-search guard, direct fallback,
duplicate-key behavior and output construction remain in place.

The trace is local to a DEX task and has no per-candidate clocks or atomics.
Group checks/matches exclude direct fallback. Final capacity sums omit growth
history and cannot be interpreted as malloc counts or peak memory. Includes'
zero vector counters describe omitted materialization, not empty intersections.
Trace logging precludes using those runs as performance evidence; measured
artifacts explicitly disable it.

The high-overlap input reaches the changed mechanism with 64 group checks and
four successes per active candidate. No-hit inputs and absent query legs stop
at the old empty-search guard and do not exercise includes. The timing boundary
includes result destruction, builder destruction and close. Checksums are a
timing guard, not a replacement for complete serialized-result checks.

## Oracle correction and one bounded addition

The initial new checker failed to compile because it attempted to add logical
children to StringMatcher. Actual StringMatcher has only value/type/ignoreCase,
and HasComposite(StringMatcher) is always false. The invalid test assumptions
were removed before any measured artifact was built. No schema or native
semantics were changed to support them. Missing batch lists/values are not
presented as supported cases because current preprocessing dereferences them.

The completed review found one valid input class absent from the initial sixteen
cases: the same normalized literal under different modes in different groups.
The native mode map is keyed by literal alone, so its final mode applies across
groups. A generic independent per-atom oracle would disagree with that existing
behavior. The requested bounded addition is Equal/Contains of `Needle` in both
request orders, preserving the old control behavior explicitly. It is not a
request to change the mode map or claim a general oracle for all collisions.

Commit `33c1ee0` adds only those checker cases and their explicit frozen expected
predicates. The extended checker is linked against the original immutable core
archives with matching compile definitions, ABI and sanitizer flags; native
libraries and measured executables remain unchanged. Source, archive, command
and checker hashes are recorded separately. This addition does not require
repeating performance measurements.

The extended 36 cases pass on small/wide control and includes, plus wide
standalone and ASan/UBSan. All six outputs agree byte for byte, and their
original 32-frame subset is unchanged. The engine's existing final-mode
behavior is preserved in both request orders. This follow-up was executed
locally after the source review; Pro did not independently run it.

The review also found the Prefix report appropriately separates long late-hit
benefits, sparse regressions, broad-positive uncertainty and absent-query
benefits. It preserves the small repeated fallback peak cost without assigning
it to nonexistent range plans, and uses corresponding Equal-only controls for
QQ's incremental PREFIX guard. That review is based on the submitted reports,
not an independent execution of the 576 Prefix samples.
