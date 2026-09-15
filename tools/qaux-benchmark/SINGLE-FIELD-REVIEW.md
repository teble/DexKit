# Single using-field source review

The authorized [Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)
completed this review in 9m 16s. The complete final response was read. Pro
traced d5a52a0 -> 008ae7e -> 2a5406d527e389ed7b14b68a241b68edeac9b52e
and read matcher/Hungarian/cross-DEX/nested reverse code, field diagnostics,
the six find-task scopes, Core CMake and both Gradle files, field fixtures,
queries, checks, workload and runner isolation changes. It did not rerun the
checks or examine later formal measurements; this is a source review.

No new engine correctness or publication blocker was identified. With one
left requirement the old solver calls the same value-taking judge in row
order and stops at the first witness. Missing inner field matchers preserve
their existing type-agnostic behavior. Empty, absent and multiple requirements
keep their original paths. Cache lookup and general preparation are removed
together, so their performance effects cannot be separated by these timings.

The concrete oracle gap was confirmed and fixed: descriptor-only reader/writer
sets could merge two unresolved fields from different DEXes. The refined
oracle retains local identities for unresolved fields and uses the unique
definition for resolved fields. The contrasting DEX 1 put-only / DEX 2 get-only
fixture passes the four normal, candidate, standalone and sanitized builds;
all earlier result sets also pass the refined oracle. The Core, measured
fixtures and executables are unchanged by this addition. Duplicate-definition
representatives remain the scope of the existing relation checker.

The report adopts the other useful limits: counters do not measure allocations
or ordered judge histories; task DEX labels include nested work; sparse still
scans the method domain; class matching includes its outer solver; short rows
do not distinguish early and late; positive_ns includes no-hit API time.

Independently, local trace preflight caught the workload's miss/missing prefix
collision. Commit c707ea9 narrowed the selected bulk family before any formal
timing. That later workload-only fix was not part of Pro's fixed source target.
The v2 workloads are linked against unchanged Core archives and retain their
own source/compiler/executable records. Both bounded corrections were checked
locally and retained in the final evidence.
