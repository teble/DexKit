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

Implementation and measurement are in progress. No review substitutes for the
local checks or artifact-specific measurements below.

## Review of R1 and initial R2 source

Pro reported reading fixed `5102357f670a8a1ab89781be0edaac6beb395fa7`, including
paged storage, descriptor parsing and generation, initialization, checks,
CMake, both Gradle files, and the R1 evidence. The complete reply was read and
copied. It did not execute R2 or infer R2 gains from R1 data.

- It found no specific new R1 identity/lookup-domain discrepancy in legal DEX
  input. It checked dense-cache publication and the R2 macro substitution.
- It identified a portability concern with deriving the body from a Record
  object's one-past pointer. The revision uses an owning char array, publishes
  a pointer derived from that array, and reads/writes the length with memcpy.
  Alignment and block growth policy stay fixed to isolate this correction.
- It identified that same-artifact descriptor comparisons were not independent
  of the R2 generator. The added dump mode freezes every class/method/field
  FlatBuffer from the all-flags-off artifact and compares complete bytes with
  each candidate, including descriptors, semantic IDs, and interface order.
- It requested exact block boundaries, oversized records, page boundaries,
  and concurrent retained reads during growth. A standalone cache checker
  adds these cases, plus empty/short/embedded-NUL records and invalid-use
  subprocess checks. ASan/UBSan supplement the explicit layout reasoning.
- For R3, it recommends resolving the TypeList once per interface matcher and
  retaining Hungarian state and positional matching. The prototype follows
  this shape and still requires an interface-heavy timing counterexample.

After rebuilding the correction with identical compiler flags, the baseline,
R1, R2, and R1+R2 native libraries are respectively byte-for-byte identical to
their earlier artifacts. The full byte comparisons and SHA-256/source mappings
are recorded in `revision-native-equivalence.json`. Earlier timed samples
therefore exercise the exact same native machine code as the corrected source;
this does not substitute for checking the revised C++ source or Android builds.
Fresh direct combination comparisons and adverse workloads follow.

## Review of corrected R2 and R3

Pro reported reading fixed `6bb4d29c45d2c134611a954b531bcdfa92152a8c`: the
corrected cache, RawTypeIds, DexItem interface paths, Hungarian/matcher changes,
workload executable, and commit delta. The complete answer was read and copied.
It did not execute tests or timing runs.

It considers the char-array record addressing to resolve the earlier specific
object-boundary concern, with no new blocker found. For R3 it checked defined
versus unresolved types, existing cross-DEX forwarding, local-ID interpretation,
interface order, positional one-to-one matching, synchronous local target
values, and raw/Bean lifetimes. It found no new semantic discrepancy. TypeList
resolution occurs once when entering the matcher and again for a successful
output Bean, not once for every pair comparison.

Two workload qualifications were adopted before collecting timings:

- Positive and negative interface requests now retain separate timings, with
  each returned buffer destroyed before its endpoint. The aggregate lifecycle
  remains measured, so a gain in one branch cannot hide the other's API time.
- The original Wide lookup has distinct names and a fast name-mismatch miss.
  Its setup generates the hit descriptor, so the first measured call is not a
  wholly cold lookup. A separate fixture adds 4,096 same-name overloads whose
  long parameter prefixes agree until the final type. A separate narrow/hot
  lookup case is also retained. Their positive and negative timings are split.

The workload harness is linked separately against the frozen native core
archives. Its own source, executable and core-archive hashes are recorded;
updating workload instrumentation does not rebuild or replace the timed core.

## JVM test execution correction

The Gradle test task can report UP-TO-DATE after a native flag change because
it does not declare the selected native library as a test input. Merely copying
its existing 71-test XML output is not new candidate validation. The experiment
now uses `force_tests.gradle` to disable that task's up-to-date/build-cache reuse,
checks that it actually executed, and saves new XML/logs for each variant.
Earlier cached invocations remain in the evidence as such; final conclusions
use the explicitly forced executions.

## Review of adverse results and bounded revisions

Pro read fixed `853b6ba`, the adverse summaries, lookup and cache source. The
complete response was read and copied. It recommends separating cross-reference
identity comparison from descriptor-input lookup, retaining cold publication,
and avoiding speculative mixed cached/raw lookup or a new global index. A zero
lookup construction count is not evidence of zero lookup calls; QQ must rerun.

For the isolated body-alignment experiment, use std::align on the actual
char-array address, keep memcpy headers and base-pointer ownership, and reserve
15 extra bytes even for oversized records. A shared positioning function must
control both fit checking and final writes. Padding, capacity and output costs
remain charged. Do not simultaneously change atomics, block sizing or serializer.
If alignment fails, a paged string cache is a possible separate candidate; its
extra allocations and indirection must be measured, not presumed harmless.

## Final narrowed-candidate review

Pro read the `8971df6` Core delta and fixed `2e8e13c` source/report. The full
reply was copied and read. It found no new correctness blocker in the cache
hit wrappers: acquire-ready proves dense optional engagement, and the returned
view refers to stable owned bytes. Paged TryGet retains both publication loads
and distinguishes an engaged empty string from a cache miss. Cold construction
and bridge destruction requirements remain unchanged.

It identified the same remaining boundary as the local review: subtracting
body minus block base can exceed PTRDIFF_MAX if a 32-bit implementation permits
an unusually large successful char-array allocation. This was not triggered by
the fixtures and does not affect the preferred variants with alignment OFF.
The correction computes the offset from capacity minus post-alignment space
minus header size, eliminating that pointer difference.

The timing evidence supports the combined revised artifact; it cannot assign
separate gains to register saves versus optional engagement checks. Failure of
this alignment experiment does not imply zero alignment effects in all contexts.
The positive prefix interval still permits about 2% slowdown, separately from
the negative-path improvement. Excluding these specific raw-lookup/R2 variants
is supported; it does not show that all arena designs must fail. Pro did not
rerun or recalculate the samples and requested no additional design direction.
