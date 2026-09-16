# Ordinary candidate execution

Initial validation: `82d6afb49d9faa5e49ee899ae948675d490ac99a`; both pipeline options remain default OFF.
Base: `0dc49fff2042cd33b91fdd97cc3813d7e50098e9`.

The [ordinary find extraction](ORDINARY-FIND-EXTRACTION.md) follow-up at
`47eb54b` removes QueryRun and its exception recovery at the user's request.
The execution contract below reflects that simplification; the completed
measurements and original source review remain tied to their recorded commits.

The user authorized the reviewed ordinary FindMethod/FindClass refactor.
The existing conservative string admission policy remains fixed. This work
does not add multi-string seed selection, predicate hoisting, a cost-based
planner, or Batch scheduling changes. Exact declaring-class, explicit scope,
and findFirst entrances retain their existing behavior.

## Execution contract

- Select the candidate source once, before executing a query's tasks.
- All-Legacy queries keep the existing submission path and consume the frozen
  decisions. The candidate coordinator is used only when at least one ordinary
  source is admitted. It activates before submitting dynamic preparation work.
- Separate candidate preparation and consumption in the interface. Legacy
  queries submit their original slices directly. Empty means proven empty,
  never an exception, cancellation, or uninitialized result.
- Keep successful preparation and validation in one worker by default.
  A separate experimental switch can split multiple occupied original ID
  ranges. One candidate, one occupied range, and one-worker execution retain
  inline validation. Inline does not mean execution on the caller thread.
- A preparation capability or budget rejection restores the original ranges.
  Previous preparation costs remain part of the query. The candidate path has
  no exception recovery policy and supports the project's no-exceptions builds.
- Candidates carry query/DEX/entity identity and enumeration coordinates.
  Method ranges use Method IDs; class ranges use ClassDef ordinals, while
  class string truth uses Type IDs. Preserve output order and final descriptor
  representative selection.
- Keep complete root-string truth separate from the candidate view. A slice
  does not truncate the proof domain. Every consuming task binds its own
  scoped proof, including the query identity.
- Existing futures wait for normal completion. Candidate task wrappers move
  their owning captures onto the worker stack, releasing them before result
  readiness. After consuming all futures, the coordinator detaches the executor
  while QueryContext is still alive. Legacy tasks retain only borrowed inputs,
  and complete their scoped query work inside the task body. Workers never wait
  for their own child tasks. Budget ownership is independent of QueryContext.
- Bound explicitly accounted arrays across preparation, futures and consumers:
  bitmap payloads, keyword-plane headers, last-string IDs, cold-index counts
  and seen IDs, the single output-group entry, class IDs, and slice records.
  Use nonblocking reservations and a bounded preparation window. This is not
  a bound on persistent indexes, matcher/trie caches, temporary associative
  containers or trie-hit buffers, result metadata, allocator retention or
  total process memory. Batch retains its current per-DEX execution and budget.
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
- [x] Verify independent ordered results, cross-DEX/recursive proof use,
  frozen decisions, failures during preparation/validation/submission, and
  one/four-worker execution. Run native, JAR/JVM and all Android ABI checks.
- [x] Freeze artifacts and compare combined execution and separately enabled
  successful re-slicing against the current B/Small engine. Validate preparation
  fallback for original ranges, order and cleanup; do not claim a measured
  production fallback speedup. Keep pure-string and one-candidate overhead,
  known regressions, cold/warm states, memory and lifecycle visible.
- [x] Review the actual implementation, archive evidence, and report the
  measured result and limitations without claiming general speedups.

These steps describe the initial implementation and validation. The follow-up
replaces exception injection checks with normal completion and retained-capture
checks, compiled without exceptions.

## Design review

The Pro design review read the fixed base and completed in 8m37s. It supported
the candidate-consumption interface, method-first migration and unchanged
Batch flow. It specifically rejected mandatory two-queue execution for every
ordinary query, and required explicit proof coverage, bounded retained
candidate storage and exception-safe task cleanup. It did not execute tests
or measure the proposed implementation.

The user subsequently rejected exception recovery for this project. The
follow-up removes that machinery while preserving normal completion and storage
lifetimes; the original review is not presented as a review of that later change.

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

## Completed evidence

[Results](CANDIDATE-PIPELINE-RESULTS.md) record 52 accepted sweeps / 624 fresh
processes, seven final native configurations, 71 JVM tests and all four Android
ABIs. The initial Android no-exceptions failure and a native benchmark adapter
deployment-target mismatch are retained separately, with their corrections.
All 22 affected native sweeps were excluded and repeated with matching compiler
settings; QQ and combined-to-slices samples were unaffected.

[Source review](CANDIDATE-PIPELINE-REVIEW.md) distinguishes the Pro-reviewed
`9227210` implementation from local follow-up fixes at `82d6afb`. Its findings
added keyword metadata accounting and failure checks using an externally owned
real scheduler. The inherited scheduler-internal OOM boundary remains outside
this refactor.

The finite comparison does not establish a QQ speedup or justify enabling
successful re-slicing: the latter raises multi-result peak footprint without a
repeatable latency benefit. Keep combined validation when using the pipeline,
and retain both experimental defaults as OFF. No admission expansion was used
to obtain the reported results.
