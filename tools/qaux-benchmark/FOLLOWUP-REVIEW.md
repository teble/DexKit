# Follow-up Pro reviews

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

## Field split design review

The complete answer was read after Pro inspected fixed `dbda5a1` through the
selected GitHub connector. This revision contained diagnostics and the plan,
not the subsequent implementation; this was an implementation-design review.

Pro confirmed the counters excluded null matcher returns and recommended
separate readiness checks without treating cache, identity and aggregation as
disjoint masks: Caller belongs to both cache and identity, and RW belongs to
cache and aggregation. Normalize complete requests before claiming missing
work, not the claimed bits, to avoid appending forward facts twice.

Retaining ordered successful field bindings was accepted as a valid design.
Pro suggested a smaller source-ID-only list as an optional later alternative;
the first prototype retains complete bindings. Identity-only/caller-only
warm-up must not discard field bindings, and aggregation must still run when
there are no new identity jobs. The existing global query/warm-up barrier
provides the required publication boundary without another lock.

Local implementation checks confirmed these properties. Tests additionally
cover original reference IDs as well as definition IDs, repeated reverse/full
calls, and controlled admission order instead of relying only on racing starts.
The suggested counterexample of unused resolved references plus concentrated
field readers/writers is implemented and has exposed the regression reported
in FOLLOWUP-RESULTS.md. This is a measured limit requiring follow-up, not a
reason to label the current version universally faster.

## Field implementation and results review

Pro read the actual `dbda5a1..064c0d8` changes and fixed `32e1f48` results,
relation checks and workload. The full answer identified no new correctness
blocker. It verified dependency normalization, masks, delayed aggregation and
the timed destruction boundaries. It correctly distinguished each executable's
internal full-first comparison from the necessary independent best5 oracle;
the latter was also executed and its full-byte hashes are retained.

The review accepted reading the local RW ready bit once in PutCrossRef, then
using short-circuit `not_ready || nonempty_get || nonempty_put` to filter only
aggregate bindings. That exact bounded correction was implemented in `bd0f8ae`.
Pro did not promise this would remove the warm regression. It recommended
splitting first/repeated API-leg timers and verifying zero warm-up jobs after
the first reverse access; `f01e59f` implements those diagnostic boundaries.

The revised checks pass, but repeatable consumer regressions remain. The
candidate is therefore classified as conditional, and the next representation
experiment proceeds independently against best5. No further field-only tuning
or universal no-regression claim is inferred from the review.

## Dense descriptor pointer review

Pro read actual fixed `5560403`, including pointer cache ownership, both
getters, macro gates, diagnostics, the dense checker and SSO workload. The
complete 6m40 response found no new correctness blocker under the existing
quiescence contract. It confirmed release/acquire, locked rechecks, exception
ownership, zero-initialized C++20 atomic pointers and independent PTR mode.

It correctly separated logical object/character capacity from allocator
rounding, and noted simultaneous method/field output buffers in the workload.
The comparison includes their destruction in both variants. Actual diagnostic
SSO counts, not length assumptions, confirm 120,000 SSO records on this host.
Pro suggested small empty/allocation-failure checks and PTR without structural
or fast-hit support; those are included in the validation follow-up. It did
not propose using QQ savings to override full-coverage regressions. The
measured decision is to exclude this prototype from the general combination.

## Contiguous invocation row review

Pro's complete 7m51 answer read fixed `b203c02` / core `a89257e`, including
the requested headers, builder, matcher, diagnostic, CMake/Gradle and four
invocation tools, plus Analyze and the relation getter checker. It found no
new correctness blocker in the current publication/borrowed-range contract.
It correctly noted that resize is not a reset operation for published rows,
and that the existing claimed-feature protocol prevents duplicate appends.

The public invoke and caller queries both request both initialization flags.
Consequently the original public ordering tests did not exercise a real
invokes-only admission. `aec239e` adds that internal sequence and a caller
queued behind a live forward admission, then checks all retained spans after
caller/full-cache publication. Its suggestion to compare ordered getters for
all raw IDs was already fulfilled by the independent relation byte oracles
for all three new fixtures. The new query checker also adds a true positional
conflict (two requirements with only one eligible target) and a single-witness
positive control. All checks pass in best5, the prototype and ASan/UBSan.

Pro verified that result and setup-local destruction enter complete lifecycle.
It distinguished persistent row compression from invoke-solver target copying,
and per-index growth overlap from process peak. Local measurements follow
those distinctions; reproducible adverse time/memory costs constrain the
decision despite the independently confirmed QQ gains.

## Raw source-file metadata review

The complete 7m03 answer read actual `7437dfa`, all requested production and
source fixture/workload files, plus raw DEX encoding and Bean serialization.
It found no new correctness blocker: sentinel handling, index zero, raw view
ownership, local defined classes and undefined cross-DEX forwarding are
preserved. The code does not add validation for invalid non-sentinel indexes.

The independent expected UTF/MUTF source bytes were confirmed. Pro correctly
distinguished within-artifact ordering checks from the external best5 oracle;
both small and wide independent byte comparisons subsequently passed. The
suggested raw sentinel/valid-empty fixture guard is added. A separate raw DEX
parse confirms sentinel 0xffffffff and empty-string index zero in both inputs.

Pro found no lifecycle timing omission and noted that source-hot costs belong
to first/repeated fields. It identified a missing explicit metrics gate in
workload_sweep. All existing timed native manifests were checked to have both
metrics options OFF, and the runner now rejects either enabled/missing option.
This is an acceptance improvement, not evidence of contaminated old samples.
Confirmed source-consumer regression and unresolved QQ time exclude this
prototype from the general preference despite its logical memory saving.

## Single-requirement and combination review

Pro's complete 6m22 answer read fixed `a02c16d` / core `c323a10`, the original
solver, matcher cache, Analyze, CMake/Gradle and the query/check/workload files.
It found no new correctness blocker. It verified original count guards,
first-witness order, Equal evaluation after the witness, positional duplicates,
cross-DEX caller dispatch and the distinct null/empty requirement behavior.
The skipped requirements cache owns pointers only; child caches still work.

It correctly limited early-mode attribution: both variants execute the same
positive query, but early and late modes use different predicates, so their
absolute difference is not the optimization benefit. Requirements-cache
lookup effects also prevent attributing the entire gain to one work array.

`645ca4f` adopts the finite test suggestions: count-allowed/rejected single
requirements; exact serial judge order and counts for late/miss/allowed/rejected
cases; and a single caller with using_fields -> get_methods/noneOf that forces
RW construction after field identity only. All 29 case buffers match the
independent original-solver control in the single and combined configurations,
including sanitizers. No production measurement binary changes: the rebuilt
single library is byte-identical. Per-case counts are logged separately.

The tiny-fixture observation needs a directional qualification. Its forward
row has one aEarly position, so duplicate-aEarly requirements fail; its caller
rows contain multiple run positions and the double-run case returns two
methods. The measured multiple modes and raw case counts retain that distinction.

Pro highlighted overlap between compact invocation storage and single matching:
both can avoid the invoke target copy. The conditional combination is measured
directly rather than adding those gains. Its field consumer and mixed-row
counterexamples remain part of the final decision; a QQ win cannot erase them.
