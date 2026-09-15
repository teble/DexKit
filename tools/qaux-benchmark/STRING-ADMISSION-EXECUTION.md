# Conservative string candidate admission

Status: implementation, validation and both measurement batches complete.
Base: `a55ba18252c642042f5831a1b4bae6c04b2719f9` (engine `cc9f893`).
Implemented engine: `7d536bf4264ab5c8deeaa4a2a3f5a4831c76576d`.
See [results and tradeoffs](STRING-ADMISSION-RESULTS.md).

The user selected admission protection and a small existing-index exception,
and explicitly deferred candidate/residual scheduling changes. The follow-up
question asks how a condition can be called cheap without knowing its result.
This experiment therefore does not rank metadata predicates by guessed cost
or selectivity. It preserves the existing pipeline when extra conditions can
make a whole-DEX string scan or serial residual matching a poor choice.

## Bounded scope

1. Ordinary unscoped, non-findFirst queries retain the global inverse path
   only when the root has usingStrings and no other constraint. Null/empty
   logical lists remain their existing no-op; other present matcher objects
   are conservatively treated as constraints. Batch is unchanged.
2. A separate, default-OFF experiment may admit an original single nonempty,
   case-sensitive ASCII Equal/StartWith root when an already published inverse
   index proves at most one posting in its range per DEX. This is an edge-count
   bound, not a selectivity prediction or a guarantee of lower latency. No
   index is built or awaited to make this decision; guarded cold queries also
   skip the optional binary-search probe. Guarded queries with two postings
   remain on the old path even when both reference the same method. Pure
   string queries retain their existing cold range and empty-range behavior.
3. Compute and capture the decision before changing the existing slice size.
   A worker must not upgrade Legacy because the range is complete or because
   another query has since built the index. Preflight bitmap budgets using
   available table sizes and conservative keyword counts. Actual index-build
   failure after pure-string admission remains an explicit existing boundary;
   this work does not add a new scheduling phase.
4. Keep the original matcher, composite prefilter, scope, result ordering,
   descriptor representative, cross-DEX resolution and admission protocol.
   This does not move names out of allOf or merge string vectors. It also does
   not assume that one candidate class has a small member-matching workload.

## Work and validation plan

- [x] Add immutable route decisions, a nonblocking acquire/release readiness
  observation, and constant-work posting-range counts using existing ranks
  and offsets. Retain the existing default-OFF inverse experiment.
- [x] Validate raw-row query results and complete ordered bytes for rare/common
  names at root/allOf, flags and return types, OR/NOT, and range bounds 0/1/2.
  Include duplicate instructions versus distinct matching string IDs,
  cold/forward-warm/inverse-warm states, and frozen admission under concurrent
  publication. Reuse existing class/cross-DEX/budget/ordering fixtures.
- [x] Run component native build, JAR, JVM tests and four-ABI Android release
  assembly with the candidate enabled. Do not change public API or schema.
- [x] Compare admission against the prior inverse artifact, then the small-range
  exception against admission, with the nine-option pipeline as a restoration
  reference. Use finite balanced process pairs and independent confirmation.
  Keep QQ totals, onInitView, the one-DEX nested guard, broad-name opportunity
  cost, small-range cold/warm API legs, lifecycle and memory separate.
- [x] Archive source identities, fixtures, commands, validations and raw timings;
  report any remaining regressions or unresolved differences. No universal
  claim that every admitted query is faster.

## Design consultation

The existing Pro conversation completed a focused review of these alternatives
after 6m39s. It recommended the conservative root-only rule plus the independent
published-index/one-posting exception. It confirmed the need for frozen worker
decisions and a synchronized readiness observation; its review did not execute
the new implementation or measurements.

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

The later 9m45s source review read the fixed base-to-7d536bf increment and found
no new production correctness blocker. Its identified warm-Range cross-DEX
coverage gap was covered by a new same-local-ID, shared-vector fixture and
checker linked against five frozen artifacts. The review did not execute the
tests or measure performance. No scheduling or predicate-hoisting work was
added.

The completed experiment has 44 sweeps / 528 accepted fresh-process samples.
One control-versus-admission broad-name reference was added in each batch after
the initial loss of inverse gains was observed. No accepted measurement was
discarded. The results retain cold-range uncertainty, broad-condition losses,
the small remaining positive-leg overhead on the old nested fixture, and the
memory cost; they do not establish a universal no-regression guarantee.
