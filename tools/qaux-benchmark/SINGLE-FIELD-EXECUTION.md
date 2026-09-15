# One using-field requirement

Status: implementation and correctness checks in progress.

1. Keep the nine-switch control and add only SINGLE_USING_FIELD to the candidate.
   The new option defaults OFF in Core, desktop and Android builds. All new
   string/batch mechanisms remain OFF for this independent comparison.
2. Before matcher-cache lookup and Hungarian preparation, handle exactly one
   raw using-field requirement by applying the existing judge in original row
   order and stopping at the first witness. Preserve absent/empty lists,
   duplicate positional requirements, Get/Put/Any, missing field matchers,
   cross-DEX identities and nested logical/reverse predicates.
3. Check independent small fixture expectations and full ordered serialized
   results across cold, full, repeat and concurrent queries. Reuse the relation
   lifecycle checks for duplicate class definitions and deferred reverse work.
   Include standalone and ASan/UBSan builds, eleven frozen QQ verification
   rounds, JVM tests and four-ABI Android assembly.
4. Use separate task-local traces to count preparation and actual judges.
   One-left Hungarian already stops at its first witness: this experiment
   does not predict fewer judges. Traces are compiled out of formal artifacts.
5. Measure finite balanced six-pair QQ and early/late/miss/sparse/multiple
   native workloads, followed by independent confirmation. Include setup,
   output destruction and close. Retain regressions and unresolved intervals.
   Review the concrete source increment in the authorized Pro conversation.

Compact forward-field rows and the reverse-only instruction walk follow this
phase as independent experiments; neither is part of this candidate.

## Progress

The normal control and candidate build successfully. The rebuilt control
native SHA256 remains 1406de12c38e356b1771209f2006c73a189975e4f90c892dcf75f48c3b4fab16.
All 24 method and 24 nested class queries pass the independent small-fixture
oracle and complete ordered bytes agree (SHA256
901032d49eb7440a836a356f70402fef41c42cc10e52edb7658e406625db74d4).
The oracle separately decodes the generated DEX instruction rows, preserving
field-use order and duplicates before evaluating the expected predicates.
Expanded fixtures, diagnostic/standalone/sanitized builds, QQ verification,
JVM/Android checks, source review and formal timing are pending.
