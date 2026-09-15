# Single using-field results

Status: complete; keep SINGLE_USING_FIELD default OFF. This independently
measured candidate reduces work and time on the bounded field fixtures, but
has no confirmed QQ query improvement. Preserve the QQ confirmation slowdown
and the repeated nested-class peak increase; do not select a production
combination from fixture wins alone.

## Control and measurements

Compare the nine-switch control with only SINGLE_USING_FIELD added. DIRECT/ID
single strings, PREFIX and batch INCLUDES remain OFF. Normal Core source is
008ae7e, unchanged through the later checker and report commits. Native SHA256:

* control: 1406de12c38e356b1771209f2006c73a189975e4f90c892dcf75f48c3b4fab16
* candidate: 3b594bb0dbafe36817b32c56e83a2f2247f38bb10840da2dde6685e81b100fc2

The corrected native workload source is c707ea9. Its executables were relinked
against the unchanged immutable Core archives into v2 artifacts; no measured
native library changed. QQ uses its original frozen queries, JAR, APK, JDK and
memory probe. All trace and internal metrics options are OFF in timed builds.

There are 18 sweeps / 216 fresh-process samples: two independently ordered
batches, six balanced pairs each, QQ at 1/11 passes and seven native workloads
at 32 repeats. Negative paired percentage changes favor the candidate. The
bracketed values are exploratory 95% paired bootstrap intervals with six
pairs, without a multiple-comparison guarantee. They do not establish zero
regression. Construction, query setup, serialized result consumption and
destruction, and bridge close are included in lifecycle. Native repeated API
is the sum of 31 warm calls; QQ stage repetition is each process's mean over
the ten warm passes, before pairing. No heavy local builds, tests, tracing or
archive jobs ran alongside formal timing.

## Native fixtures

Small has 51 methods / 223 field uses. Short and long each have 3,627 methods,
with 7,231 and 921,631 field uses respectively. Bulk families have 600 methods
per source DEX in each of early/late/miss; the remaining methods are semantic
counterexamples. The long late alpha is penultimate, followed by beta. The
short group is an early-hit check; at two uses its early and late rows coincide.

| Workload | Main lifecycle | Confirmation lifecycle | Confirmation repeated API |
| --- | --- | --- | --- |
| 256 uses, first alpha | -33.95% [-40.12, -30.93] | -33.61% [-34.00, -31.19] | -39.51% [-39.92, -38.47] |
| 256 uses, penultimate alpha | -26.71% [-31.35, -23.99] | -26.38% [-28.16, -25.54] | -28.74% [-30.40, -27.82] |
| 256 uses, no alpha | -34.44% [-35.99, -33.04] | -34.50% [-35.82, -33.97] | -37.58% [-38.73, -36.23] |
| Name-filtered two methods | +0.54% [-1.54, +12.06] | -2.46% [-7.44, +5.26] | +3.65% [-8.69, +19.48] |
| Two requirements (unchanged solver) | -0.89% [-1.61, +0.55] | +0.43% [-1.71, +2.16] | -0.25% [-1.30, +1.89] |
| Nested class member query | -5.33% [-7.78, -0.01] | -6.04% [-10.24, -2.06] | -34.70% [-41.71, +40.48] |
| Two uses, first alpha | -30.51% [-34.10, -25.66] | -29.19% [-33.65, -26.60] | -30.22% [-34.33, -27.57] |

The long early lifecycle medians in confirmation are 34.634 -> 23.248 ms;
its warm API medians are 27.858 -> 16.836 ms. Ratios of marginal medians are
not the paired percentage estimator shown above.

Sparse here means a method-name filter admits only two methods to field
matching; the enclosing query still scans the full candidate domain and cold
preparation builds all forward field rows. The class workload includes the
unchanged outer member solver. Its lifecycle/API improvement does not imply
a precise improvement in its very short warm phase: that warm interval crosses
zero in both batches. Multiple requirements retain the general field solver.

No stable process-peak reduction is established. The nested-class peak cost
repeats: main +0.97% [+0.43, +1.22], confirmation +0.97% [+0.24, +1.16]. The
confirmation marginal medians are 13,484,992 -> 13,599,680 bytes. This remains
an observed cost, without attributing it to a particular allocator mechanism.
Short peak estimates are negative in both batches, but confirmation includes
zero. The main multiple-requirement peak increase does not repeat decisively.

## Frozen QQ

| Batch | Whole lifecycle | Relevant optional_has_info API | Peak footprint |
| --- | --- | --- | --- |
| main, 1 passes | +2.05% [-4.27, +8.79] | +1.71% [-2.43, +5.35] | +0.17% [-0.20, +0.46] |
| main, 11 passes | -0.41% [-3.06, +0.69] | -0.00% [-1.20, +1.78] | -0.18% [-0.67, +0.23] |
| confirm, 1 passes | +0.30% [-2.86, +0.73] | -1.87% [-13.20, +0.54] | +0.12% [-0.34, +0.44] |
| confirm, 11 passes | +2.37% [+0.22, +4.45] | +1.60% [-1.38, +4.72] | +0.11% [-0.66, +0.27] |

The relevant cold API also remains unresolved in 11-pass processes: main
-0.30% [-6.61, +8.58], confirmation +0.29% [-3.43, +8.72]. Its confirmation
warm medians are 20.329 -> 20.613 ms. The full 11-pass confirmation lifecycle
medians are 5,933.803 -> 6,059.488 ms, with a positive paired interval. Main
11-pass lifecycle was -0.41% [-3.06, +0.69], so the slowdown is not replicated
across both batches; retain the warning signal without claiming it is either
proved persistent or mere noise. No whole-QQ or relevant-query gain is proved.

## Separate work counts

Two-pass native traces show identical calls, eligible rows, row lengths and
judge counts between control and candidate. For each long method family:
2,400 single requirements contain 614,400 total target items. Early executes
2,400 judges, late 612,000, and miss 614,400. The control constructs 2,400
solvers and records 614,400 prepared target items; the candidate records zero
solvers, cache builds and prepared items for those single requirements. The
multiple case retains 2,400 solvers, 614,400 prepared items and 616,800 judges.

QQ reaches only 29 single requirements / 41 field uses per pass, all under
ReplyNoAtHook/optional_has_info. Judges remain 41; solvers fall from 29 to zero
and the matcher-vector cache factory from one to zero. This is a small part of
the roughly 20 ms warm API, consistent with its unresolved measured change.

These counters are not allocation counts or a peak-memory forecast.
row_items counts whole encountered rows; prepared_items approximates target
copy elements and excludes requirement vectors, matrices and allocator work.
judges includes nested field-use matching. The task's DEX label need not be
the DEX of every nested field. Source review establishes one-left judge order;
the trace measures counts, not an ordered judge-event history. Avoid assigning
all measured gains to copies alone: cache lookups and generic solver setup
also disappear. Native positive_ns is total query API time even for miss;
negative_ns is an unused zero placeholder.

## Correctness and build evidence

The 24 method plus 24 nested class queries compare independent expectations
and complete ordered FlatBuffers bytes across cold, full, repeated and
concurrent execution. The independent oracle first decodes actual DEX code
rows, including duplicates, order and code presence. Small/short/long normal
controls agree. Small covers both traced builds, standalone and ASan/UBSan;
long also covers standalone and ASan/UBSan. Two existing relation fixtures
exercise duplicate definitions, unresolved cursors, late reverse publication,
queued reverse work and forward/reverse/full concurrency, with identical bytes
in both traced builds and the sanitized candidate.

After Pro review the oracle distinguishes unresolved fields by DEX/local ID
and resolved fields by their unique definition. A new small fixture writes an
unresolved field only in DEX 1 and reads it only in DEX 2. All four normal,
candidate, standalone and sanitized results agree, SHA256
dd00e1a972782d5170d4e07bc9aecc90ae6d37fbf0d14555cdac2b454f7b7a62.
The six original small/short/long result sets also pass the refined oracle.
This model intentionally rejects duplicate definitions; existing relation
checks cover that separate behavior. No Core or measured input changed.

Both normal native libraries pass eleven unchanged QQ verification rounds.
The required Gradle cmakeBuild, jar, test and Android assembleRelease checks
pass with the candidate plus nine controls: 71 JVM tests, zero skips/failures,
and arm64-v8a/armeabi-v7a/x86/x86_64 AAR libraries. The JAR hash is unchanged.

A preflight workload assertion caught a prefix collision before measurement:
"miss" also admitted the deliberately positive "missing" semantic method.
The corrected bounded bulk prefix "miss0" is used in every formal sample.
The first trace parser expected JSON stage lines; it was corrected to associate
counters with the adapter's existing pass/stage text and frozen report order.
The successful raw traces were retained, and the completed parser verifies
all stage bindings and equal control/candidate judge counts. These setup
failures did not become performance data.

See SINGLE-FIELD-REVIEW.md for the reviewed scope and the bundled evidence
manifest for raw samples, verification, build identities, traces and scripts.
Continuous forward-field rows and the reverse-only instruction walk remain
separate subsequent experiments against the same nine-switch control.
