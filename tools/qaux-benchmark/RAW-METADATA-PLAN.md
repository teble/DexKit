# Raw metadata experiments

## Objective and scope

The user authorized a new round of Pro-guided architecture experiments after
the first three hypotheses were completed. Seek lower complete-workflow time
and/or lower peak memory, preferring memory savings without time regression.
Existing representations are candidates for replacement, not fixed design
requirements. The earlier phase's three-hypothesis limit and 10%/15% expansion
guideline do not define the acceptance threshold of this round.

Work stays on `codex/raw-metadata-experiments`, based on `42e6c7f`. The original
checkout is separate. Review pushes, when needed, go only to `teble/DexKit`.
The three earlier experiments remain OFF during isolated comparisons.

## Plan and current state

1. **In progress:** obtain Pro's complete advice against the fixed source and
   prepare a fresh baseline/calibration using the existing verified corpus.
2. Select concrete independent candidates from the advice, check their source
   assumptions, and add default-OFF prototype switches. Record each candidate's
   representation, avoided work, expected costs, and counterexamples before
   looking at its performance samples.
3. Compare exact results and targeted lifetime/concurrency cases, then measure
   complete short and reused lifecycles. Do not time builds or diagnostics
   concurrently with formal samples.
4. Independently confirm useful candidates; investigate actual regressions.
   Review the resulting source and evidence with Pro when it can resolve a
   concrete design or correctness question. Report supported tradeoffs and
   remaining uncertainty, without requiring a successful optimization.

## Measurement contract

Reuse QQ 9.3.55 and QAuxiliary `01801ffd`, the frozen full result/flow contract,
arm64 JBR 17.0.6, fixed 256 MiB JVM heap, four engine threads, and the existing
native builder and memory probe. A new native baseline is built from the phase
base with all experiments OFF. Historical scores are context, not the control
for new measurements. Preserve every valid sample and immutable artifact.

Calibrate with six paired A/A runs for one and eleven passes. For admitted
candidates use twelve balanced AB/BA pairs in both modes, followed by six new
confirmation pairs for useful results. Targeted counterexamples are separate
from QQ scores. Report construction, first pass, repeated API sum, close,
complete lifecycle, and physical footprint; native logical storage is only
attribution. Preserve workload-specific peaks and memory after close.

Time preference is not a claim that zero regression can be statistically
proved. Report paired changes and uncertainty. Distinguish clear regressions,
supported improvements, and intervals that cannot resolve a tradeoff. Do not
interpret failure to reject a difference as proof of non-inferiority, select a
regression margin after seeing samples, or keep sampling until a result wins.
If a user-facing time budget is needed for adoption, report the measured upper
bound/tradeoff rather than inventing an accepted budget.

## Facts to preserve

- Raw string bytes and the fixed DEX ID tables are already shared views. A
  second compressed string copy does not remove the retained raw image.
- Descriptor strings currently support cross-DEX resolution, descriptor
  lookup, result deduplication, serialization, and stable `string_view` storage.
- Different DEX IDs do not imply different textual symbols. Descriptor-based
  deduplication and current output order must remain equivalent.
- Relationship order, duplicates, empty rows, nested matching witnesses,
  publication/close coordination, and returned value lifetimes must be kept.
- All conversion, allocation, growth, synchronization, rebuilding, and cleanup
  costs belong in the complete comparison. Prior arena experience showed that
  faster destruction alone can still make the whole workflow slower.

## Pro consultation

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

The new request was sent once against fixed source `42e6c7f`, asking for actual
source inspection, alternatives to descriptor materialization, and immediate
bounded tests of the most promising representations. Status: waiting for the
complete answer. No new candidate has been attributed to Pro yet.
