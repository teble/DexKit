# Pro review: raw metadata experiments

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

## Initial design review

The complete response to the new request was read, including the alternatives,
counterexamples, and references. Pro reported examining `teble/DexKit` at
`42e6c7f3dfe3769d4d48d49345f38b0c609030ef`: descriptor construction, cross-DEX
resolution, descriptor lookup, result collection/deduplication, Beans,
matchers, Reader access, and default-OFF CMake switches. It did not rerun the
benchmark or independently recalculate the earlier census.

The local source checks support the consequential observations:

- Cross-reference equality and descriptor lookup currently materialize full
  signatures. Raw member/prototype tables already carry their components.
- The cross-reference loop advances its source cursor only on a match. An
  unresolved reference can block later references. The performance prototype
  must preserve this existing progression and representative selection.
- Ordinary method/field searches deduplicate by complete signature, not by
  the local DEX/member ID pair. Their external descriptor format remains used
  by Kotlin sorting, equality, type/name access and reflection wrappers.
- Interface IDs are copied into persistent vectors despite an existing raw
  TypeList. Hungarian then copies its target vector into matching state.

## Adopted experiments

1. Compare raw member components and parse lookup input once. Keep the output
   cache in the first measurement to isolate avoided internal materialization.
2. Test a compact paged cache of stable byte records. This retains reuse and
   string_view lifetime, avoiding the proposed immediate switch to result-owned
   strings that could repeatedly rebuild large outputs. Charge all page/block,
   alignment, synchronization, growth and cleanup costs.
3. Read interface IDs through a raw count/index view, preserving one-to-one
   matching and avoiding an added temporary vector in each query.

The agent additionally identified that removing eager cross-reference string
construction leaves more output slots cold. R1 therefore explicitly publishes
immutable cached strings with ready bytes and cold-fill locks. This cost is
part of the candidate and is exercised by cold concurrency checks.

The advice to select a tolerance before interpreting measurements is not user
authorization to accept a particular regression. The user's preference remains
no time regression; measurements report changes and upper uncertainty bounds,
and label unresolved tradeoffs rather than asserting statistical equivalence.

Other suggested offset/view compression candidates remain separate follow-ups;
they are not mixed into these prototypes. No blanket arena substitution or
global symbol interning table was adopted.

## Validation status

Implementation and measurement are in progress. This initial review covers the
source base and design, not a review of later prototype source or a guarantee
of performance. Subsequent source review and actual evidence will be recorded
here when complete.
