# Ordinary candidate execution

Status: implementation in progress.
Base: `0dc49fff2042cd33b91fdd97cc3813d7e50098e9`.

The user authorized the reviewed ordinary FindMethod/FindClass refactor.
The existing conservative string admission policy remains fixed. This work
does not add multi-string seed selection, predicate hoisting, a cost-based
planner, or Batch scheduling changes. Exact declaring-class, explicit scope,
and findFirst entrances retain their existing behavior.

## Execution contract

- Select the candidate source once, before executing a query's tasks.
- All-Legacy queries keep the existing submission path and consume the frozen
  decisions. The new query run is used only when at least one ordinary source
  is admitted. It activates before accepting work so exceptional cleanup
  never needs to start a partially submitted queue.
- Separate candidate preparation and consumption in the interface. Legacy
  queries submit their original slices directly. Empty means proven empty,
  never an exception, cancellation, or uninitialized result.
- Keep successful preparation and validation in one worker by default.
  A separate experimental switch can split multiple occupied original ID
  ranges. One candidate, one occupied range, and one-worker execution retain
  inline validation. Inline does not mean execution on the caller thread.
- A preparation capability or budget rejection restores the original ranges.
  Previous preparation costs remain part of the query. Exceptions propagate
  after accepted tasks have been drained; they are not negative results.
- Candidates carry query/DEX/entity identity and enumeration coordinates.
  Method ranges use Method IDs; class ranges use ClassDef ordinals, while
  class string truth uses Type IDs. Preserve output order and final descriptor
  representative selection.
- Keep complete root-string truth separate from the candidate view. A slice
  does not truncate the proof domain. Every consuming task binds its own
  scoped proof, including the query identity.
- A query run owns the executor and tracks accepted work through capture
  cleanup. Activation, sealing, draining and detachment are distinct events.
  Workers never wait for their own child tasks. Budget ownership is independent
  of QueryContext so late destruction cannot access a released context.
- Bound candidate working arrays across preparation, futures and consumers;
  use nonblocking reservations and a bounded preparation window. This is not
  a bound on persistent indexes, result metadata, allocator retention or total
  process memory. Batch retains its current per-DEX execution and budget.
- The initial coordinator uses a bounded FIFO preparation window of at most
  twice the configured worker count. Head-of-line waiting is a performance
  tradeoff to measure. Result concatenation moves Bean payloads rather than
  copying them; combined-path changes must not be attributed solely to slicing.

## Work plan

- [x] Add the query-run lifetime and candidate-storage contracts, plus focused
  checks for failure, cleanup, identity, original ranges and reservations.
- [x] Migrate ordinary FindMethod candidate production and consumption without
  changing admission or matcher semantics; validate before the class path.
- [x] Migrate FindClass with explicit ClassDef ordering and complete Type-ID
  truth. Preserve Batch and the special query entrances.
- [ ] Verify independent ordered results, cross-DEX/recursive proof use,
  frozen decisions, failures during preparation/validation/submission, and
  one/four-worker execution. Run native, JAR/JVM and all Android ABI checks.
- [ ] Freeze artifacts and compare interface-only combined execution,
  preparation fallback, and separately enabled successful re-slicing against
  the current B/Small engine. Keep pure-string and one-candidate overhead,
  known regressions, cold/warm states, memory and lifecycle visible.
- [ ] Review the actual implementation, archive evidence, and report the
  measured result and limitations without claiming general speedups.

## Design review

The Pro design review read the fixed base and completed in 8m37s. It supported
the candidate-consumption interface, method-first migration and unchanged
Batch flow. It specifically rejected mandatory two-queue execution for every
ordinary query, and required explicit proof coverage, bounded retained
candidate storage and exception-safe task cleanup. It did not execute tests
or measure the proposed implementation.

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3
