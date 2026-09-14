# First research phase

## User-approved scope

- Use the pinned QQ APK and extracted QAuxiliary queries as an engine benchmark.
- Query-result equivalence is sufficient for this benchmark. Do not add host
  reflection or hook initialization as acceptance requirements.
- Iterative consultation with the already-bound Pro conversation is authorized
  for advancing experiments, challenging hypotheses and interpreting results.
  Ordinary in-scope iterations do not require a new approval each time.
- Keep experimental code in the existing isolated worktree. Pro advice is
  evidence to evaluate; it is not an additional source of authorization.

The user authorized this phase and created its Goal on 2026-09-15 (Asia/Shanghai).
Experiment branches and commits are permitted. Review pushes are allowed only to
`git@github.com:teble/DexKit.git`; never push to LuckyPray/DexKit. The bound Pro
conversation remains https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3.
No automatic merge or release is in scope.

## Objective

Establish a reproducible performance baseline for the fixed query corpus, then
test up to three prioritized architecture/performance hypotheses through
isolated, switchable experiments. Deliver a supported decision for each tested
hypothesis, reproducible measurements and any promising prototype. Preserve
query behavior and existing regression tests. A supported negative finding, or
a clearly bounded inconclusive result, is a valid research output.

Use the earlier compact-storage, nested-execution and bounded-memoization ideas
as candidate directions. First establish where time and memory go; select the
initial hypothesis set based on those observations and a Pro critique. Do not
require all three architectures to be implemented in full.

## Equivalence contract

- Freeze APK, corpus, feature order, initial cache state and baseline outputs.
- Compare every executed stage's semantic identifiers, result count and
  multiplicity. Compare ordering wherever it affects a selected first result.
- Preserve selected descriptors, uniqueness outcomes and the conditional path
  through dependent queries. Use actual intermediate results as downstream
  inputs.
- Preserve expected empty results. A prematurely aborted, failed, timed-out or
  canceled query is not an equivalent empty result.
- Keep descriptor/class-name checks that decide later query execution. These
  checks are part of the replay control flow and do not require host reflection.
- Keep existing semantic tests as regression coverage. Result equality here is
  relative to the frozen baseline, not a claim of universal engine correctness.
- Make any intentional baseline/query correction separately visible; never
  update expected results merely to make an optimization pass.

## Measurement and iteration

1. Separate output verification from timed runs and reduce logging/hash overhead.
   Measure create, first workflow, repeated workflows, close and complete
   lifecycle; capture memory with the measurement scope clearly stated.
2. Establish process/JIT/cache conditions and run-to-run variability. Alternate
   baseline and candidate runs rather than comparing isolated lucky samples.
3. For each hypothesis, state its mechanism, predicted observation and a result
   that would contradict it. Change one principal mechanism at a time.
4. Verify results before interpreting performance. Keep candidate preparation,
   cache construction, fallback work and cleanup costs in the comparison.
5. Send Pro a bounded update containing real measurements, relevant source or
   readable changes, and unresolved alternatives. Read its full answer, check
   important claims locally, and choose the next useful experiment.
6. Record each experiment's source state, input hashes, settings, measurements
   and disposition. Routine build fixes do not need a Pro consultation.

## Proposed decision thresholds

Use a repeatable reduction of at least 10% in complete-workflow time or 15% in
peak memory as an initial threshold for prioritizing a larger implementation.
The other principal metric and key query paths should have no material
regression outside measured noise. These are proposed engineering thresholds,
not observed improvements or required outcomes for completing the research.
If baseline noise is too large to resolve them, repair the measurement method
before interpreting a candidate result.

## When this phase ends

- The baseline is reproducible and the selected hypothesis set has been
  evaluated, with supported, rejected or explicitly inconclusive dispositions.
- A promising candidate has independent confirmation runs, equivalent results,
  appropriate component checks, a recorded diff and a clear scope of benefit.
- A route with repeated targeted attempts but no new evidence is closed as
  unproven for this workload, rather than expanded indefinitely.
- The final package identifies the best observed option, tradeoffs, rejected
  ideas and unresolved limits. New architectures become a subsequent phase.

A Pro reply agreeing with an idea, one fast run, or reaching a budget limit is
not proof of completion. If a required input or environment is unavailable,
report the specific missing dependency and preserve the unfinished work; do not
report the research objective as achieved.
