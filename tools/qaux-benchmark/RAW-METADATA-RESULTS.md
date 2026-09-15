# Raw metadata experiment results

The historical prototypes and rejected variants below retain their original
source/artifact identities. Final QQ comparisons of the narrowed candidates
are in progress; the final recommendation will use those new measurements.

## Historical R1 bundle: cross-reference and raw input lookup

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

The later adverse workload sections show why this original bundle was revised.
Its QQ benefit does not establish a universal adoption decision.
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

## Adverse workload findings before tuning

The bounded native-only workloads reveal regressions that QQ does not expose.
These prevent treating the combined switches as a general no-regression choice.
Every comparison below contains six balanced fresh-process pairs, with all
valid samples retained. They link the frozen `6bb4d29` core with the split-timer
workload at `cba4e7d`. Full results are in `evidence/raw-metadata/workload-*`.

| Comparison and workload | Complete lifecycle change [95% interval] | Repeated API change [95% interval] |
| --- | ---: | ---: |
| R1, large repeated output | -0.05% [-6.58, +1.91] | -0.12% [-6.14, +1.58] |
| R1, distinct-name wide lookup | -15.74% [-18.04, -12.46] | +12.88% [+10.68, +20.37] |
| R1, same-name long-prefix lookup | +169.69% [+168.45, +173.31] | +180.78% [+179.66, +185.04] |
| R1, narrow/hot lookup | +24.76% [+22.55, +29.55] | +26.71% [+24.37, +31.84] |
| R2, large repeated output | +15.11% [+14.60, +17.64] | +17.63% [+17.15, +20.42] |
| R1 to R12, large repeated output | +14.19% [+8.07, +17.84] | +16.91% [+10.58, +20.78] |
| R3, interface queries | +0.05% [-0.59, +1.03] | +0.12% [-1.09, +1.15] |

R1's same-name-prefix miss also has a different total descriptor byte length,
which lets the legacy cached-string path reject on length before comparing
bytes. The raw path repeatedly walks the shared parameter prefix. This is a
valid adverse input but is not an equal-length, last-byte mismatch experiment.
The narrow/hot case additionally exposes parsing and raw-comparison overhead.
The next bounded revision will separate cross-reference identity optimization
from lookup behavior instead of bundling both into an adoption claim.

R2 improves first output and close but regresses repeated large output. Its
records in this fixed long-descriptor fixture place character bodies eight
bytes off a 16-byte boundary, unlike the baseline allocations. Whether body
alignment explains the regression is a hypothesis to test, not a causal claim.
A focused storage revision will be checked before final combined QQ runs.

Positive and negative interface times are individually retained: the isolated
R3 intervals are [-1.26%, +2.18%] and [-1.89%, +1.89%] respectively. This fixture
has not resolved a small speed tradeoff; it does not prove zero regression.

## Cross-reference-only revision

`aae3956` separates raw descriptor-input lookup into a default-OFF switch. R1
alone now retains legacy cached-text lookup and still publishes cold descriptors
safely. Six balanced pairs at seed 2026091514 produce the following. Positive
values are regressions; all samples, including an output outlier, are retained.

| Workload | Lifecycle change [95% interval] | Repeated API change [95% interval] | Negative request change [95% interval] |
| --- | ---: | ---: | ---: |
| lookup | +7.46% [+4.22, +12.27] | +11.06% [+5.45, +19.04] | +12.49% [+10.92, +13.19] |
| lookup-hot | +1.43% [+1.07, +3.04] | +1.40% [+1.19, +3.17] | +3.74% [+2.74, +7.11] |
| lookup-prefix | +0.49% [-1.08, +3.26] | +0.63% [-1.13, +3.22] | +12.97% [+10.77, +15.05] |
| output | +0.73% [+0.24, +15.30] | +1.06% [+0.04, +11.18] | n/a |

The catastrophic prefix and hot regressions shrink substantially, but warm
negative lookup still regresses. This revision is not yet a general no-time-
regression choice. The ready-byte acquire and extra optional-value check are
present in its hot generated code; no single cause is established by timings.

## Isolated body alignment and final cache-hit revision

The aligned/unaligned pair uses the same `534e225` source and differs only by
`DEXKIT_EXPERIMENT_ALIGNED_DESCRIPTORS`. Both retain char-array ownership,
checked offsets and atomic publication. Alignment changes actual record-body
addresses, not just header strides. ASan/UBSan cache checks pass both layouts.

- workload-align-r2-unaligned-v3-output: lifecycle -0.17% [-1.83, +0.30].
- workload-align-workload-control-v2-output: lifecycle +15.62% [+13.83, +17.40].

Alignment does not resolve the output regression. `8971df6` separately adds
`DEXKIT_EXPERIMENT_DESCRIPTOR_FAST_HITS`: a short published-cache hit wrapper
with a distinct cold construction function. It retains ready-byte acquire or
two-level acquire publication; it does not remove synchronization. The dense
ready byte proves optional engagement, so this path avoids a redundant checked
optional access. The following comparisons are against the frozen baseline.

| Candidate/workload | Lifecycle change [95% interval] | Repeated API change [95% interval] | Negative request change [95% interval] |
| --- | ---: | ---: | ---: |
| r1-fast/lookup | -18.37% [-22.63, -9.12] | -27.19% [-32.48, -14.13] | -44.99% [-45.89, -44.44] |
| r1-fast/lookup-hot | -2.81% [-3.46, -1.63] | -3.18% [-3.94, -1.87] | -11.27% [-12.57, -8.79] |
| r1-fast/lookup-prefix | -0.22% [-1.23, +0.88] | -0.37% [-1.82, +0.71] | -42.98% [-46.51, -39.23] |
| r1-fast/output | -3.89% [-11.48, -0.64] | -4.15% [-11.77, -0.89] | n/a |
| r2-fast/lookup | -14.82% [-15.57, -13.55] | +2.69% [-0.15, +3.01] | +36.35% [+33.96, +43.47] |
| r2-fast/lookup-hot | +2.04% [+1.61, +3.58] | +2.35% [+1.29, +3.96] | +13.32% [+11.05, +16.03] |
| r2-fast/output | +16.33% [+12.91, +18.02] | +18.75% [+14.77, +20.80] | n/a |

These six-pair exploratory results favor the revised R1 over the regressing raw
lookup bundle. Some positive-path confidence intervals still admit about 2%
slowdown; no universal zero-regression guarantee follows. R2 still regresses
complete repeated output and hot negative lookup, so it is excluded from the
preferred final combinations. The final measured candidates retain the dense
output-string cache, R1 fast hits and R3 borrowed interface IDs. A separate
combination additionally enables H1/H2/H3. No further storage tuning is planned.

## Final component validation

Every narrowed-source native variant passed both independent complete
FlatBuffer oracles: normal and same-name-overload three-DEX fixtures. The
all-flags-off and R3 libraries are byte-identical to the earlier frozen controls.
The codegen extract records short 13-instruction dense hit wrappers and
20-instruction paged hit wrappers; this is a mechanism observation, not a
separate estimate of how much each code change contributes to timing.

R1-fast, R2-fast, R1-fast+R3, H1/H2/H3+R1-fast+R3, and all nine flags each
completed native build, JAR, an explicitly executed 71-test JVM suite with
zero failures/errors/skips, and release AAR builds for all four Android ABIs.
The all-flags combination passed exhaustive metadata, memo and allocation-budget
checks plus ASan/UBSan symbol and paged-cache checks. Prior cached Gradle
invocations are not counted as these executions.

`325eb97` then replaced the aligned-record pointer difference with size_t
remaining-space arithmetic, covering the reviewed PTRDIFF_MAX edge. The
corrected all-nine-flags build passed both frozen oracles, ASan/UBSan symbol and
cache checks, another fresh 71-test JVM suite, and all four release AAR ABIs.
The preferred timed artifacts remain pinned to `8971df6`/`2e8e13c` with alignment
OFF; this correction is inside the excluded alignment branch. It does not
assign new measurements to a changed binary. No Android device timing is
claimed by these host tests and packaging checks.

Full component logs, manifests, byte-equivalence records, oracle hashes and
codegen extracts are under `evidence/raw-metadata`. Final QQ verification is
performed after this last JAR build, before collecting paired measurements.
