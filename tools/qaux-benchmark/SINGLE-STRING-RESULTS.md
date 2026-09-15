# Ordinary single-string experiment results

## Equal: completed

Both single-Equal paths reduce the actual QQ SettingEntry API cost. DIRECT
is the smaller implementation; ID gives an additional, reproducible improvement
in that API and in long-reference workloads, at a measurable cost in sparse
queries. No universal winner is selected. Both switches remain default OFF,
and Contains/multiple requirements keep their original semantics and path.

The incremental control is the existing nine-switch combination, not master.
Native production code is fixed at `1b9dd37`; control, DIRECT and ID native
SHA256 values are respectively `2347a204...`, `d259bdc0...`, `5a800346...`.
Full identities, raw measurements, validation and fixtures accompany this
report under `evidence/next-round/strings-equal`.

Each comparison has two independent batches of six balanced fresh-process
pairs. All 48 sweeps / 576 process samples are retained. Timed builds have
trace, diagnostics and both internal-metrics options OFF. QQ uses the frozen
9.3.55 APK, original query adapters, four threads and the same JAR, JDK and
memory probe. Native fixtures include creation, setup, result serialization
and destruction, and close. OS file caches are not flushed. These are macOS
ARM64 measurements; Android verification is build coverage, not a runtime
performance claim. Intervals below are exploratory paired-median bootstrap
95% intervals; negative changes favor the second implementation.

### Frozen QQ replay

The confirmation batch relative to the nine-switch control:

| Boundary | DIRECT | ID |
| --- | ---: | ---: |
| One round, create through close | -3.18% [-10.17, +1.70] | -2.02% [-4.01, +3.81] |
| Eleven rounds, create through close | -11.27% [-12.11, -11.05] | -9.75% [-11.57, -6.96] |
| Repeated complete query APIs, rounds 2-11 | -14.01% [-14.49, -12.74] | -13.43% [-14.99, -8.87] |
| SettingEntry first Equal API | -74.84% [-75.96, -73.98] | -78.92% [-79.02, -78.60] |
| SettingEntry repeated Equal API | -75.27% [-75.66, -72.57] | -79.18% [-79.57, -77.47] |

The first independent eleven-round batch also improves lifecycle by 11.24%
with DIRECT and 11.66% with ID. One-round lifecycle improvement does not
reliably repeat, and neither candidate has a confirmed process-peak memory
improvement. The control comparisons above have separate paired samples;
subtracting their percentages is not a DIRECT-versus-ID comparison.

The actual head-to-head confirmation finds ID reduces the SettingEntry
repeated API by another **15.09% [-20.09, -13.53]** relative to DIRECT, roughly
20 ms to 17 ms per call. This extra API improvement is present in both batches.
Its effect on the whole eleven-round lifecycle is **-0.56% [-2.81, +3.47]**,
following -2.71% in the first batch; an additional whole-workload benefit is
not consistently distinguished. One-round head-to-head lifecycle intervals
also cross zero.

### Controlled reference workloads

Three DEXes, 1,500 bulk methods per DEX, 16 ordered references per bulk row and
70,000 padding strings per DEX exercise actual jumbo IDs. The 210-byte Equal
needle shares all but its final byte with bulk fillers. Early and late layouts
have exactly one witness at positions 0 and 15; the miss layout has no bulk
witnesses. Four fixed correctness methods still match in the miss fixture.
Each iteration also runs a query whose needle is absent from every pool.

Confirmation lifecycle changes against AC:

| Workload | DIRECT | ID |
| --- | ---: | ---: |
| Early long Equal, 16 iterations | -93.82% | -94.26% |
| Late long Equal, 16 iterations | -89.03% | -94.12% |
| Bulk miss, 16 iterations | -93.26% | -98.76% |
| Sparse declared class, 16,384 iterations | -47.50% | -38.97% |
| Class usingStrings, 16 iterations | -98.89% | -99.02% |

These deliberately concentrated native workloads are not estimates of whole
QQ or general application gains. Their direct-versus-ID comparisons expose
the cost tradeoff more clearly:

| ID relative to DIRECT | First batch | Confirmation, with interval |
| --- | ---: | ---: |
| Early long Equal | -5.16% | -4.85% [-6.49, -3.88] |
| Late long Equal | -46.77% | -46.94% [-47.49, -46.67] |
| Bulk miss | -81.82% | -81.44% [-81.58, -81.16] |
| Sparse declared class | +15.42% | +16.11% [+13.79, +17.61] |

Sparse iterations amortize creation so that the additional directory/search
cost is visible. The sparse query visits only one DEX, but its ID plan owns
entries for all three. ID therefore remains a conditional option; it is not
automatically selected for every single-string request.

Contains and two-requirement controls still use AC. Confirmation lifecycle
changes are DIRECT +0.69% [-0.03, +2.24] / ID +0.40% [-0.48, +0.91] for Contains,
and DIRECT -0.44% [-1.13, +1.15] / ID +1.50% [-8.05, +8.64] for multiple
requirements. The first Contains/DIRECT batch showed a small +0.41% interval
above zero; the second does not resolve it. This is not proof of zero gating
cost or a reason to remove the control results.

### Mechanism and correctness

Separate diagnostics show that one QQ Equal query reaches 2,049,726 method
matchers. AC visits 2,071,884 string references, DIRECT 2,071,882 and ID 55,665.
Thus early exit saves only two references in this real query; DIRECT's main
gain comes from avoiding AC/hit-set work. ID can reject a DEX whose pool has
no matching ID before visiting its method reference rows; this explains the
large reduction in references and must not be attributed solely to integer
comparison. ID creates one query-owned plan with
3,976 bytes of object/entry storage and 41 ranges, using 656 comparisons and
1,691 decoded UTF-16 units. This storage figure excludes allocator overhead
and shared cache containers. It is not a whole-process memory saving.

The corrected implementation passes the independent complete 68-query
wide ordered-result oracle, all UTF-16 unit range checks, eight concurrent
callers at DEX IDs 0 and 65536, ID ASan/UBSan, both 71-test JVM configurations,
both four-ABI Android builds and eleven frozen QQ verification rounds. The
initial small-input checks also passed. The narrowing and fixture-witness bugs were fixed before formal timing.
See SINGLE-STRING-REVIEW for review scope and evidence limitations.

## StartWith: completed

Both paths avoid substantial ordinary single-prefix AC work. ID is faster
for a long prefix with late witnesses and slower for sparse access. A broad
prefix does not show a confirmed additional lifecycle benefit over DIRECT.
Both implementations remain default OFF with no automatic dispatch threshold.

The same nine-switch AC control is byte-identical (`2347a204...`). DIRECT+PREFIX
and ID+PREFIX are `53e87cea...` and `ac756bc2...`; production source is unchanged
since `1b9dd37`, with diagnostic-only pool counters added at `d67fe1c`.
All 48 sweeps / 576 samples, identities, checks and fixtures are retained in
`evidence/next-round/strings-prefix`. The measurement boundaries and exploratory
interval method are the same as Equal above: two batches of six balanced
fresh-process pairs, including construction, preparation, output and close.

### Positive, absent and complete workload costs

The long prefix has 210 ASCII bytes. Each bulk witness is a distinct proper
extension with a DEX/method suffix, with first/last positions asserted during
fixture generation. The broad 11-byte `LongPrefix/` prefix matches every bulk
reference, not the entire pool. Sparse and class queries use six-byte `Needle`.
Each iteration runs the positive query followed by a completely absent query.
The bulk-miss fixture still contains four fixed positive correctness methods.

Confirmation changes relative to AC are shown as DIRECT / ID:

| Workload | Positive APIs | Fully absent APIs | Create through close |
| --- | ---: | ---: | ---: |
| Early long prefix, 16 iterations | -90.42% / -90.98% | -99.41% / -99.56% | -93.80% / -94.15% |
| Late long prefix, 16 iterations | -82.06% / -91.00% | -99.34% / -99.56% | -89.25% / -94.17% |
| Bulk miss, 16 iterations | -89.31% / -99.45% | -99.55% / -99.66% | -93.59% / -98.71% |
| Broad prefix, 16 iterations | -92.76% / -92.81% | -99.42% / -99.57% | -94.79% / -94.87% |
| Sparse declared class, 16,384 iterations | -52.31% / -38.61% | -52.36% / -35.54% | -47.46% / -34.04% |
| Class usingStrings, 16 iterations | -99.58% / -99.69% | -99.71% / -99.92% | -98.80% / -99.00% |

These are concentrated native fixtures, not projected QQ/application gains.
The head-to-head samples provide the actual ID-versus-DIRECT comparison:

| ID relative to DIRECT | First lifecycle batch | Confirmation lifecycle, with interval | Confirmation positive API |
| --- | ---: | ---: | ---: |
| Early long prefix | -6.07% | -7.51% [-8.78, -5.82] | -6.92% [-8.21, -5.21] |
| Late long prefix | -45.29% | -45.60% [-46.10, -33.38] | -49.65% [-50.08, -37.62] |
| Broad prefix | -0.94% | -1.72% [-4.83, +0.91] | -0.02% [-2.24, +1.04] |
| Sparse declared class | +29.68% | +25.63% [+24.87, +27.47] | +29.92% [+27.26, +31.52] |

Broad-prefix repeated combined APIs improve by 1.76% [-3.94, -0.83], while the
positive API remains unresolved; its absent query improves by 35.13%. Thus this
small combined gain is not evidence that integer comparison improves broad
positive matching. Sparse fully absent APIs also regress by 35.04% with ID:
with so few reference visits, directory/range construction can cost more than
the scan it replaces. No runtime crossover threshold is selected from this
limited matrix.

### Unchanged workload guards

QQ contains no explicit StartWith query. Its PREFIX-on comparison therefore
uses the corresponding Equal-only build as control. Confirmation eleven-round
lifecycle is DIRECT +1.55% [-1.59, +3.24] and ID -0.15% [-1.64, +3.30]; repeated
API changes are +0.68% [-0.82, +3.13] and +0.03% [-1.93, +2.38]. The first
lifecycle batch is +0.37% / -1.75%, both intervals crossing zero. DIRECT's
first-batch first API increase does not repeat. No stable extra QQ lifecycle
gain, regression or peak-memory improvement is established.

Multiple prefixes, mixed Equal/Contains requirements and pure Contains retain
AC. Their confirmation lifecycle changes are respectively DIRECT +4.04%
[-3.84, +9.24] / ID +0.72% [-3.43, +2.43], DIRECT -0.37% [-2.14, +0.80] / ID
-0.19% [-14.24, +1.17], and DIRECT +0.15% [-3.68, +0.54] / ID +0.79%
[-0.30, +9.67]. These intervals do not prove zero routing cost.

There is a small repeated process-peak cost in the ID multiple-prefix fallback:
+1.85% [+0.48, +5.30] initially and +1.92% [+0.96, +2.51] in confirmation.
Diagnostics create no single-string plans in this workload, so this measurement
is retained without attributing it to range storage. Some other memory deltas
appear in only one batch; all are available in the raw summaries. The affected
single-prefix synthetic workloads reduce peak footprint against AC, but that
does not establish a whole-QQ saving or a universal ID memory advantage.

### Work counts and validation

Two-iteration diagnostics produce four query-owned plans and twelve ranges
for early/late long-prefix methods. Their ID reference visits are 9,224 versus
144,224, with 384 pool comparisons and 19,626 decoded units. The summed pool
count is 877,236 and summed matched range length 9,012. These counts sum over
range constructions, not globally unique strings. Both layouts use 1,312
logical plan/entry bytes across their four plans, excluding allocator and
shared-container overhead. Completely absent ranges skip reference rows;
the positive query and absence optimization must be distinguished.

The broad case has summed matched range length 9,132 and 1,146 decoded units.
Sparse queries construct only four ranges in the visited DEX, although each
plan owns a three-DEX directory. Multiple-prefix and mixed controls have zero
single-string plans/ranges and preserve AC results. Trace is disabled for all
formal timings.

The independent oracle passes 45 method and 45 class cases on small and wide
pools for control, DIRECT and ID, including standalone builds without the nine
control switches. The original 68-result subset is byte-identical to the Equal
oracle. ID ASan/UBSan, all UTF-16 ordering checks, concurrent DEX-ID isolation,
both 71-test JVM runs, both four-ABI AAR builds and eleven frozen QQ verification
rounds pass. Desktop Gradle native hashes match the measurement artifacts.
There is no Android runtime performance claim. The source review scope is
recorded in SINGLE-STRING-REVIEW.
