# Deferred RW instruction-walk results

Status: complete. SKIP_RW_WALK reproducibly reduces QQ's first deferred field
reverse operation by roughly one third. The one-pass lifecycle improvement
also repeats; the eleven-pass whole-workload effect remains unresolved. No
stable peak-memory improvement is established. Keep the option default OFF,
preserve the forward-only guard slowdown, and do not add these percentages to
the earlier independent experiments.

## Identity and measurement boundaries

Production source is fixed at b57b687afa84bf3315b84204a2d49e3c84ec1dc1. Both
variants enable the nine existing controls. The candidate adds only
DEXKIT_EXPERIMENT_SKIP_RW_WALK; COMPACT_FIELDS, SINGLE_USING_FIELD, newer single
strings, PREFIX and batch INCLUDES remain OFF. Normal native SHA256 values:

* control: 1406de12c38e356b1771209f2006c73a189975e4f90c892dcf75f48c3b4fab16
* candidate: f122713430d844b33cae9a6498722654cb07a4bf3aee65b576e8012967701955

Fourteen sweeps retain all 168 fresh-process samples: six balanced pairs for
each workload, followed by an independently ordered confirmation batch. All
timed artifacts disable diagnostics, traces and internal metrics. QQ uses the
frozen 9.3.55 APK, original query adapters, four threads, unchanged JAR/JDK and
memory probe. The added final operation targets MessageRecord.msg and compares
all 399 readers and 313 writers with the independent ordered control.

Lifecycle includes construction, query preparation, complete output handling,
destruction and close. Native full-first initialization is inside setup, and
the remaining query/metadata destruction is inside the outer lifecycle. These
are macOS ARM64 timings; Android ABI assembly establishes build coverage only.
OS file caches are not flushed. No heavy build, test, trace or archive job ran
alongside formal timing.

Percentages are medians of paired changes; brackets are exploratory 95%
bootstrap intervals from six pairs, without a multiple-comparison guarantee.
They are not ratios of separately reported marginal medians. Negative favors
the candidate. Complete records are in evidence/next-round/rw-walk.

## Frozen QQ with a real final RW operation

| Boundary | Main batch | Confirmation |
| --- | ---: | ---: |
| One pass, final RW API | -37.07% [-39.06, -34.20] | -38.47% [-39.54, -36.81] |
| One pass, create through close | -4.33% [-5.92, -2.00] | -3.32% [-4.43, -1.82] |
| One pass, peak footprint | -0.01% [-0.49, +0.38] | +0.03% [-0.26, +0.27] |
| Eleven passes, final RW API | -36.23% [-39.91, -32.84] | -33.32% [-43.03, -12.11] |
| Eleven passes, create through close | -2.21% [-3.21, +0.15] | -0.05% [-2.26, +6.84] |
| Eleven passes, peak footprint | +0.20% [+0.10, +0.44] | +0.10% [-0.32, +0.29] |

Confirmation marginal medians for final RW are 156.956 -> 96.849 ms after
one pass and 157.768 -> 105.730 ms after eleven. The one-pass lifecycle medians
are 1,858.734 -> 1,793.860 ms. The eleven-pass medians are 5,996.526 -> 6,061.751
ms; the paired estimate and its wide interval do not establish an overall
improvement there. Retain this unfavorable absolute comparison.

The final operation includes field lookup, the first readers call, then a
writers call that uses the now-ready reverse tables. Its confirmation detail:

| API boundary | After one pass | After eleven passes |
| --- | ---: | ---: |
| Field lookup | +9.29% [-12.53, +35.87] | +3.94% [-31.94, +23.86] |
| First readers, including deferred preparation | -39.19% [-40.43, -37.37] | -33.68% [-44.06, -12.64] |
| Following writers | -16.31% [-42.24, +7.73] | +35.81% [+2.17, +77.97] |

First-reader improvement repeats in the main batch (-37.78% / -36.86%).
Its confirmation marginal medians are 153.771 -> 94.001 ms and 154.104 ->
102.565 ms. The eleven-pass writer increase is retained: 0.787 -> 0.951 ms,
following a main paired estimate of +1.93% [-31.62, +27.61]. It is not a
replicated warm-path change and cannot be attributed directly to the deleted
one-time scan. Peak memory likewise has no consistent change: eleven-pass
confirmation medians are 1,734,186,336 -> 1,735,808,480 bytes. The initial
small positive peak interval does not repeat decisively.

## Original QQ and native lifecycle guards

Original QQ does not request the deferred RW operation. Its lifecycle changes
are +0.23% [-4.31, +5.03] / +1.44% [-3.34, +10.55] for one pass, and -1.33%
[-3.87, -0.15] / +0.36% [-3.11, +2.49] for eleven passes (main / confirmation).
Neither establishes a repeated lifecycle gain or regression. Repeated APIs
and process peak also remain unresolved. The initial eleven-pass gain is not
assigned to a branch this original replay does not enter.

The native field-adverse fixture has three DEXes, 134 methods with code,
50,576 instruction-loop steps and substantial repeated field output. Each
process runs 32 iterations. In the late case, only the first reverse operation
can skip the scan; 31 later reverse calls still produce their full outputs.

| Native mode | Main lifecycle | Confirmation lifecycle | Confirmation first reverse API |
| --- | ---: | ---: | ---: |
| Forward only | +2.82% [-6.35, +15.26] | +5.15% [+1.43, +8.74] | Not requested |
| Forward then late RW | -0.47% [-1.66, +0.27] | -0.36% [-1.07, +0.83] | -6.82% [-12.99, -2.67] |
| Full initialization first | +0.19% [-1.20, +13.74] | -0.11% [-1.88, +1.19] | -6.03% [-9.59, +11.10] |

The late first-reverse confirmation medians are 5.413 -> 5.024 ms, following
-3.37% [-16.77, +3.80] initially. Thus even this small first-call improvement
is not resolved in both batches. Whole native lifecycle and peak effects
remain unresolved. Warm reverse sums have changes near zero with intervals
crossing zero. Full-first's initial first-reverse decrease (-12.53%) does not
repeat decisively and occurs after initialization, outside the changed branch.

The forward-only guard is an unfavorable signal: confirmation lifecycle
medians rise from 5.841 to 6.193 ms, and its repeated API increases 6.08%
[+3.87, +10.77], versus +3.37% [-20.92, +14.09] initially. No RW-bearing cache
call occurs in that workload. Retain both batches and avoid labeling the
increase either a proved persistent regression or harmless noise. No candidate
is selected for universal deployment from these results.

## Actual work removed and unchanged storage

Separate diagnostics count the existing width-walk loop, not timing:

| Workload | Eligible calls | Control code-method visits / loop steps | Candidate visits / steps |
| --- | ---: | ---: | ---: |
| QQ final RW | 41 | 1,921,832 / 40,384,589 | 0 / 0 |
| Native late RW | 3 | 134 / 50,576 | 0 / 0 |
| Native full-first (mixed extraction) | 0 | 134 / 50,576 | 134 / 50,576 |
| Native forward-only | 0 | No RW-bearing call | No RW-bearing call |

Full-first has three mixed RW-bearing calls, which both variants still scan.
The joint RW+caller schedule also produces exactly three eligible 0x1400
calls per fixture. Control walks 1,200 steps on the small relation fixture and
50,576 on field-adverse; diagnostic and sanitized candidates walk zero.

Both variants have byte-for-byte equal logical storage censuses for forward
fields, readers and writers. For QQ, reader row index/payload capacities remain
35,828,856 / 37,585,880 bytes, and writer capacities 35,828,856 / 13,206,512 bytes.
This deletion changes neither table representation nor final capacity.

rw_only means no new instruction-derived output is missing; a call may also
request caller relations or annotations. instructions counts width-walk steps,
including payload entries, not CPU instructions or 16-bit code units. A
code-less DEX may legitimately count zero even in control. walk=false leaves
the separate reverse fill's method/edge traversal intact. The log is emitted
before reverse filling and publication, so it is not a completion signal.

## Correctness and reviewed boundaries

All normal small/short/long/unresolved fixtures pass 24 method and 24 nested
class queries plus complete GetUsingFields output, independently checked from
actual decoded DEX rows and ordered serialized controls. Cold/full/repeated/
concurrent results agree. Small/long/unresolved also pass standalone and
ASan/UBSan candidates. Standalone validates compatibility with eager field
coupling; the actual deferred branch is covered with FIELD_IDENTITY_SPLIT ON.

Both relation fixtures pass all seven lifecycle schedules in diagnostic
control/candidate and the sanitized candidate. After Pro review, the checker
adds readiness assertions before and after joint RW/caller admission. All six
reruns pass and preserve the original complete ordered bytes. Only the checker
was recompiled and linked to unchanged immutable Core archives after timing;
no measured library or workload executable changed.

Both normal libraries pass eleven frozen QQ rounds with and without final RW.
The candidate completes required desktop native/JAR builds, all 71 JVM tests
with no failures/errors/skips, and four-ABI Android AAR assembly. The JAR hash
is unchanged. The actual source review found no new production-path blocker;
RW-WALK-REVIEW.md records its scope and adopted diagnostic/coverage limits.

This completes the final mechanism in NEXT-ROUND-EXECUTION.md. The measured
benefit is conditional on deferred RW preparation; no new combination or
automatic production-enablement decision is implied.
