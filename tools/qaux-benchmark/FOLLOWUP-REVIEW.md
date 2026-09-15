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
