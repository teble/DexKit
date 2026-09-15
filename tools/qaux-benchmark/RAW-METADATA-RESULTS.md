# Raw metadata experiment results

This round is in progress. R1 has completed its initial validation and paired
confirmation. R2 and R3 are not yet performance conclusions.

## R1: structural member identities

R1 replaces descriptor-string equality in cross-DEX resolution and descriptor
lookup with comparisons of raw member components. It preserves the original
search domain, cursor progression, representative selection, result ordering,
textual deduplication and output descriptor cache. Since more output descriptors
remain cold, ready bytes and striped cold-fill locks publish their strings.
Their 4,176,868 diagnostic bytes are included in the candidate's memory costs.

The native artifact is from `e8e7b4b`, SHA-256
`9fba70a8a31f276823367cf0ef28fa2669080c19aa3228ca4df2c1a4e4e36333`.
All older prototype switches are OFF. Baseline and R1 use the same compiler,
runtime, Java artifact and pinned input within each comparison. Complete
eleven-pass verification passed for both artifacts after the Gradle build.

Values are median paired percentage changes, with exploratory paired-bootstrap
95% intervals. Main cohorts have twelve balanced AB/BA pairs; independent
confirmation has six. Negative changes favor R1.

| Cohort | Passes | Complete lifecycle | Peak physical footprint | Repeated API sum |
| --- | ---: | ---: | ---: | ---: |
| Main | 1 | -12.83% [-17.49, -11.05] | -6.36% [-6.47, -6.29] | N/A |
| Confirmation | 1 | -12.30% [-16.09, -5.04] | -6.56% [-6.75, -6.20] | N/A |
| Main | 11 | -3.87% [-4.88, -3.27] | -6.24% [-6.52, -6.09] | -0.04% [-0.82, +0.37] |
| Confirmation | 11 | -4.65% [-5.47, -2.27] | -6.53% [-6.86, -6.45] | -0.30% [-0.51, +1.19] |

Both lifecycles improve in both cohorts. Repeated API times remain close to
zero change; these intervals do not prove zero regression for warmed queries.
First-pass API sums improve about 9.5--10.3%, and close improves about 22--25%.
Creation does not show a consistent benefit. These are fixed QQ workflow
measurements on macOS arm64, including the JVM, not Android runtime results.

## Attribution

The diagnostic one-pass executions preserve the frozen results. The equality
attempt counts remain 1,503,972 methods and 418,558 fields. This supports that
the prototype changed representation/comparison, not the traversal extent.

| Diagnostic observation | Baseline | R1 |
| --- | ---: | ---: |
| Materialized descriptor slots | 1,317,599 | 1,628 |
| Descriptor character-buffer capacity | 130,809,332 B | 196,369 B |
| Dense optional-slot array storage | 130,972,800 B | 130,972,800 B |
| First builds during cross-reference resolution, methods | 1,003,554 | 0 |
| First builds during cross-reference resolution, fields | 312,423 | 0 |
| First builds on output paths, methods | 1,622 | 1,628 |

The six additional output-path builds replace signatures previously created
during cross-reference resolution. Diagnostic materialized-byte counts are
logical signature lengths, not measured CPU time or physical memory traffic.
The census excludes some allocator and object overhead and is not a complete
process memory account. Diagnostics are compiled out of timed artifacts.

## Correctness and component checks

- Native compilation, JVM JAR, all 71 JVM tests, and Android release AAR builds
  pass with R1 ON and the older flags OFF.
- The generated metadata-only DEX fixture contains 4,127 method IDs and 15
  field IDs over three DEXes. It includes same-name overloads, return-type
  distinctions, array parameters/order, non-ASCII names, differing local IDs,
  missing references before resolvable later references, duplicate definitions,
  and a class with 4,096 long-signature methods.
- Baseline and R1 pass 8,810 full-text-oracle identity comparisons, including
  52 equal symbols with differing local IDs. Lookup domains, missing/malformed
  inputs, duplicate representatives, full result sets and signature deduplication
  remain equivalent. The existing unresolved-reference cursor behavior is
  explicitly preserved, not silently fixed.
- Retained native views survive cache growth and full warm-up. R1 is exercised
  with eight concurrent cold readers including the same slot; the legacy
  control uses warm reads because it lacks cold-write publication.

The separate adverse workload timings are still to be completed with R2/R3.
R1's QQ result is supported, but it is not yet a universal adoption decision.
All prototype switches remain OFF by default.

## Evidence

Committed samples, summaries, artifact manifests, diagnostics and check logs
are under `evidence/raw-metadata/`. Full process reports, GC logs, build logs,
verification output and copied R1 JUnit results are in the external
`qq-9.3.55/raw-metadata/formal` data directory. No valid slow sample was removed.
