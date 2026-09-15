# Raw metadata experiment results

## Final decision

For the frozen QQ 9.3.55 workload, the preferred measured combination is H1/H2/H3
plus raw cross-reference identities with fast descriptor cache hits (R1-fast)
and borrowed raw interface lists (R3). Keep raw descriptor-input lookup, paged
byte descriptors (R2), and body alignment OFF. All experiment switches still
default to OFF; this branch does not change the default library behavior.

The complete combination improves both lifecycle lengths and peak physical
footprint in the main and independent confirmation cohorts. On confirmation,
one-pass lifecycle improves 18.90% and eleven-pass lifecycle improves 12.36%;
peak physical footprint improves 15.14% and 14.97%, respectively. The repeated
API sum in the eleven-pass run improves 9.75% [-11.36%, -8.70%]. Whole-process
median peaks are 2002.49 -> 1698.53 MiB (one pass) and 2014.17 -> 1712.01 MiB
(eleven passes): about 302--304 MiB less, including the fixed JVM workload.
These are direct combined comparisons, not added isolated percentages.

The new R1-fast + R3 pair alone saves about 7.3--7.7% of process peak footprint.
Its one-pass lifecycle improvement repeats, but its warmed timing remains
unresolved: the eleven-pass confirmation lifecycle interval is [-6.56%, +1.53%]
and repeated API interval is [-1.99%, +5.29%]. R1-fast alone likewise leaves a
+4.12% upper repeated-API bound in confirmation. These are not time-free memory
optimizations in a universal sense. R3 alone reliably saves about 1% of peak
footprint here; the measurements do not establish a repeatable speed benefit.

R2 is excluded because its corrected paged-byte cache still regresses repeated
large output by about 16% in complete lifecycle. Body alignment did not resolve
that counterexample. Raw descriptor-input lookup is excluded because its
same-name long-prefix and narrow/hot lookups regressed. Restoring cached-text
lookup plus the short published-cache hit wrapper resolves the large observed
lookup regressions without weakening cold publication.

## Final QQ paired measurements

Every candidate has twelve balanced fresh-process main pairs and six separate
confirmation pairs, for one and eleven passes. Negative changes favor the
candidate. Cells are median paired percentage changes with exploratory paired
bootstrap 95% intervals; they are not percentage changes of column medians.
No valid sample was discarded. The control is the original all-flags-off native
machine code, with the same JAR, runtime, adapter, input hashes and memory probe
within each comparison. Timings include create, queries, result destruction and
close; warmed API sums cover the ten repeated passes.

| Candidate | Cohort | Passes | Complete lifecycle | Peak physical footprint | Repeated API sum |
| --- | --- | ---: | ---: | ---: | ---: |
| R3 | main | 1 | +0.07% [-2.64, +2.41] | -1.00% [-1.23, -0.82] | N/A |
| R3 | main | 11 | -0.99% [-3.29, +0.69] | -1.02% [-1.20, -0.89] | -0.14% [-2.14, +0.23] |
| R3 | confirm | 1 | -0.42% [-1.64, +0.35] | -0.89% [-1.17, -0.63] | N/A |
| R3 | confirm | 11 | -3.33% [-8.81, +0.06] | -1.07% [-1.30, -0.66] | -3.65% [-8.57, +0.34] |
| R1-fast | main | 1 | -12.60% [-13.18, -10.25] | -6.34% [-6.50, -6.17] | N/A |
| R1-fast | main | 11 | -3.89% [-5.10, -2.06] | -6.45% [-6.77, -6.21] | +0.03% [-1.07, +0.67] |
| R1-fast | confirm | 1 | -13.54% [-16.90, -7.51] | -6.50% [-6.86, -6.11] | N/A |
| R1-fast | confirm | 11 | -4.73% [-5.23, -1.34] | -6.24% [-6.46, -6.15] | +1.11% [-0.32, +4.12] |
| R1-fast + R3 | main | 1 | -12.53% [-15.63, -11.74] | -7.31% [-7.55, -7.15] | N/A |
| R1-fast + R3 | main | 11 | -2.71% [-3.93, -2.24] | -7.40% [-7.63, -7.24] | +0.34% [-0.26, +1.25] |
| R1-fast + R3 | confirm | 1 | -13.94% [-17.11, -13.08] | -7.71% [-7.93, -7.10] | N/A |
| R1-fast + R3 | confirm | 11 | -3.14% [-6.56, +1.53] | -7.53% [-7.82, -6.97] | +1.13% [-1.99, +5.29] |
| H1/H2/H3 + R1-fast + R3 | main | 1 | -18.34% [-19.97, -17.39] | -15.19% [-15.31, -15.01] | N/A |
| H1/H2/H3 + R1-fast + R3 | main | 11 | -13.69% [-14.68, -12.50] | -15.09% [-15.35, -14.94] | -9.69% [-11.92, -9.18] |
| H1/H2/H3 + R1-fast + R3 | confirm | 1 | -18.90% [-21.98, -17.77] | -15.14% [-15.31, -14.95] | N/A |
| H1/H2/H3 + R1-fast + R3 | confirm | 11 | -12.36% [-13.21, -11.53] | -14.97% [-15.33, -14.43] | -9.75% [-11.36, -8.69] |

## Final native counterexamples and interpretation limits

The combined variants were also tested directly against the control with six
balanced pairs per native fixture, including result destruction and close.
These are separate synthetic workloads, not QQ or Android runtime scores.

| Candidate / workload | Lifecycle | Positive request | Negative request | Peak physical footprint |
| --- | ---: | ---: | ---: | ---: |
| R1-fast + R3 / output | +0.01% [-2.22, +1.92] | N/A | N/A | +0.77% [-1.12, +3.91] |
| R1-fast + R3 / lookup | -18.98% [-23.90, -13.28] | -12.44% [-20.34, -4.61] | -44.90% [-46.62, -43.56] | +0.17% [+0.17, +0.59] |
| R1-fast + R3 / lookup-prefix | -1.81% [-3.60, -0.16] | -0.69% [-2.46, +1.01] | -45.12% [-46.33, -42.88] | -0.61% [-0.75, -0.34] |
| R1-fast + R3 / lookup-hot | -3.46% [-7.51, -2.16] | -1.60% [-2.60, -0.47] | -11.98% [-24.68, -9.63] | +2.30% [+1.91, +3.06] |
| R1-fast + R3 / interfaces | -0.40% [-4.28, +1.54] | -0.33% [-4.09, +2.31] | -1.05% [-4.58, +1.07] | +1.41% [+0.00, +1.76] |
| H1/H2/H3 + R1-fast + R3 / output | +0.46% [-10.25, +0.91] | N/A | N/A | +6.50% [+0.64, +9.91] |
| H1/H2/H3 + R1-fast + R3 / lookup | -22.79% [-26.33, -18.90] | -17.92% [-22.48, -12.17] | -45.22% [-47.07, -44.64] | -2.10% [-2.43, -1.68] |
| H1/H2/H3 + R1-fast + R3 / lookup-prefix | -1.57% [-2.43, -0.08] | -0.25% [-1.18, +1.18] | -44.30% [-46.17, -41.14] | -2.71% [-2.92, -2.51] |
| H1/H2/H3 + R1-fast + R3 / lookup-hot | -3.18% [-4.39, +0.35] | -1.26% [-2.86, +3.54] | -12.02% [-13.04, -10.46] | -9.57% [-9.96, -8.45] |
| H1/H2/H3 + R1-fast + R3 / interfaces | -2.05% [-2.81, -0.53] | -1.21% [-3.10, +0.33] | -1.99% [-3.36, -0.70] | -9.22% [-10.45, -8.48] |

The full combination's repeated large-output time is near the baseline, but its
small-fixture peak footprint increases 6.50% [+0.64%, +9.91%]. The positive
narrow/hot interval also admits +3.54% time regression. The prefix positive
interval admits +1.18%, independently of the much faster negative request.
These limitations remain in the decision; QQ gains do not cancel a different
workload's regression. An interval crossing zero is not proof of equivalence,
and no chosen slowdown tolerance or Android-device performance guarantee is
implied. The prefix miss has a different total byte length; its result cannot
be relabeled an equal-length last-byte mismatch test.

## Final storage attribution

The new R1-fast and combined artifacts retain 1,628 descriptors, compared with
1,317,599 in the control. Cross-reference comparisons remain 1,503,972 methods
and 418,558 fields. Cross-reference signature construction falls from 1,003,554
methods plus 312,423 fields to zero. Lookup construction is zero in this corpus;
that is not evidence of zero lookup calls. The six additional output-path builds
replace descriptors previously constructed while resolving cross references.

| Storage observation before close | Control | R1-fast + R3 | Full combination |
| --- | ---: | ---: | ---: |
| Dense descriptor slots | 130,972,800 B | 130,972,800 B | 130,972,800 B |
| External descriptor character capacity | 130,809,332 B | 196,369 B | 196,369 B |
| Descriptor publication guards | 0 B | 4,176,868 B | 4,176,868 B |
| Class-member index arrays | 49,971,888 B | 33,314,592 B | 33,314,592 B |
| Class-member payload capacity | 18,459,808 B | 17,512,000 B | 17,512,000 B |
| Unused lazy opcode/string directories | 83,200,992 B | 83,200,992 B | 0 B |
| Method-string index arrays | 62,400,744 B | 62,400,744 B | 31,200,372 B |
| Method-string payload capacity | 10,561,680 B | 10,561,680 B | 11,436,032 B |

R3 removes 17,605,104 B of persistent interface index/payload storage and
229,475 nonempty interface allocations in this corpus. H2's larger payload
capacity is retained in the accounting; its lower index cost and buffer count
are separate observations. Logical storage counts do not equal process peaks
or CPU traffic, and sums exclude some allocator/object overhead. Descriptor
character capacity excludes small-string storage inside the string object.
The diagnostic builds are distinct from every timed artifact.

## Reproducibility and completion

All five final native artifacts passed full eleven-pass frozen-result
verification after the last JAR build. All 288 timed child processes then used
matching verified native/JAR/runtime/input/probe fingerprints, preserved counts,
selections and query control flow, and exited successfully. Full verification
materializes result identities; formal timing does not add that hashing work.
The paired samples and summaries, complete per-child runtime records and
verification reports are committed under `evidence/raw-metadata`.

`narrowed-run-records.tar.gz` contains the full records; `narrowed-run-records.json`
indexes every member's SHA-256 and the verified inputs. Native manifests,
independent compressed FlatBuffer oracles, component logs and the review record
are alongside it. The preferred native sources are pinned to `8971df6`/`2e8e13c`;
`325eb97` only corrects the excluded alignment branch. Artifact hashes, not the
latest branch name, identify the measured machine code. Reproduction commands
and all default-OFF flag mappings are in this directory's README.

The planned implementations, checks, adverse tests and final comparisons are
complete. Android release AAR compilation and host tests passed; no Android
device timing or host reflection/hook compatibility beyond the agreed result
contract is asserted. Main checkout and default switches remain unchanged.
The sections below retain historical prototypes, rejected hypotheses and their
source-specific measurements; they are not scores for the current preferred
configuration.

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
codegen extracts are under `evidence/raw-metadata`. Final QQ verification was
performed after this last JAR build, before collecting paired measurements.
