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
gain comes from avoiding AC/hit-set work. ID creates one query-owned plan with
3,976 bytes of object/entry storage and 41 ranges, using 656 comparisons and
1,691 decoded UTF-16 units. This storage figure excludes allocator overhead
and shared cache containers. It is not a whole-process memory saving.

The corrected implementation passes the independent complete 68-query
wide ordered-result oracle, all UTF-16 unit range checks, eight concurrent
callers at DEX IDs 0 and 65536, ID ASan/UBSan, both 71-test JVM configurations,
both four-ABI Android builds and eleven frozen QQ verification rounds. The
initial small-input checks also passed. The narrowing and fixture-witness bugs were fixed before formal timing.
See SINGLE-STRING-REVIEW for review scope and evidence limitations.

## StartWith: active

The next isolated comparison enables the existing PREFIX switch. Proper
extensions of the long needle, a broad prefix, sparse access, class matching
and overlapping multiple prefixes have separate controlled workloads. The
QQ adapter has no explicit StartWith query, so its check compares PREFIX ON
against the corresponding Equal-only implementation to detect added overhead;
Equal's QQ improvement must not be attributed to StartWith.
