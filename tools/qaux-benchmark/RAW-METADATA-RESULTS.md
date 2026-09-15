# Raw metadata experiment results

This round is in progress. R1, R2, and the R2 increment over R1 have completed
paired measurement and confirmation. R3 and direct combinations are pending.

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

## R2: paged stable byte records

The optional string arrays are replaced by 256-slot atomic pages and stable
char blocks owned by eight cold-fill stripes. Each record stores its byte
length followed by the final descriptor and NUL. Misses size and write directly
from raw parts, with checked bounds and release publication. Blocks grow from
4 KiB to 64 KiB; oversized records get sufficient space. Warm views survive
growth and require no reconstruction. All allocation, padding, tails, locks,
output serialization, and destruction remain part of the comparison.

R2 and R1+R2 were first measured from `5102357`. The source-level correction
in `6bb4d29` replaces a Record-tail address with pointers into an owning char
array and memcpy length headers. Rebuilding with the same compiler/settings
produces exactly the same full native library bytes, documented in
`evidence/raw-metadata/revision-native-equivalence.json`. These measured machine
code artifacts therefore also correspond to the corrected source. Independent
byte-oracle and sanitizer checks validate the revised implementation separately.

| Comparison | Cohort | Passes | Complete lifecycle | Peak physical footprint | Repeated API sum |
| --- | --- | ---: | ---: | ---: | ---: |
| Baseline to R2 | main | 1 | -11.87% [-13.35, -9.96] | -5.20% [-5.43, -5.07] | N/A |
| Baseline to R2 | main | 11 | -2.88% [-5.63, -2.06] | -5.16% [-5.29, -4.91] | -0.07% [-2.52, +0.87] |
| Baseline to R2 | confirm | 1 | -11.56% [-15.50, -5.95] | -5.11% [-5.38, -4.92] | N/A |
| Baseline to R2 | confirm | 11 | -4.21% [-5.18, -2.75] | -5.30% [-5.63, -4.77] | -0.21% [-1.29, +0.12] |
| R1 to R1+R2 | main | 1 | -1.83% [-5.98, -0.22] | -6.92% [-7.05, -6.76] | N/A |
| R1 to R1+R2 | main | 11 | -1.16% [-2.36, -0.34] | -6.77% [-6.85, -6.65] | -0.05% [-0.88, +0.36] |
| R1 to R1+R2 | confirm | 1 | -3.68% [-8.35, -0.29] | -6.82% [-7.40, -6.23] | N/A |
| R1 to R1+R2 | confirm | 11 | -1.05% [-2.69, +1.03] | -6.94% [-7.35, -6.64] | -0.17% [-1.28, +1.61] |

R2 independently lowers complete lifecycle time and peak footprint in both
cohorts. Added to R1 it consistently saves another roughly 6.8--6.9% of process
footprint. Its incremental time improvement is small; the eleven-pass
confirmation interval permits +1.03% complete-time regression and +1.61% warm
API regression. This is not evidence that adding R2 is universally time-free.

The diagnostic R1+R2 cache holds 1,628 records: 2,303,328 B of index/ownership
storage and 1,183,744 B of record capacity. Of that capacity 208,549 B is used,
including 4,649 B of alignment padding; 975,195 B remains block-tail space.
Headers plus character bytes occupy 203,900 B. There are 1,009 pages and 289
blocks. These costs are small relative to the R1 dense array and publication
guards, but sparse use of a small DEX can still pay page/stripe/block overhead.

Standalone R2 retains the original 1,317,599 materialized records. Its index
occupies 30,943,584 B; block capacity is 162,226,176 B, with 142,069,715 B used
and 137,520,049 B in headers plus character bytes. Logical cache totals do not
equal process peaks. Instrumented lock-wait sums are diagnostic wall-clock
samples, not production CPU costs.
