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
