# Results: fixed QQ / QAuxiliary architecture experiments

The finite phase is complete. Three switchable prototypes preserve the pinned
query results. H1 gives the clearest memory reduction; H3 gives the clearest
improvement when the same workflows repeat. H2 reduces storage overhead but
has a small whole-process effect. None independently reaches the predeclared
10% lifecycle or 15% peak-memory guideline for expanding the architecture.
All three remain OFF by default on the experimental branch.

## Decisions

| Hypothesis | Supported observation | Disposition |
| --- | --- | --- |
| H1: allocate lazy directories on demand | About 6% lower process footprint; lifecycle changes are small and vary between cohorts | Retain the isolated prototype; do not extend to paging or lazy/full ownership migration in this phase |
| H2: contiguous method string-use storage | About 1.6--1.8% lower process footprint; buffer count falls from 552,516 to 123 | Representation mechanism supported, limited measured benefit; do not generalize to all relations |
| H3: bounded memo of empty method-batch trie parses | 11-pass lifecycle improves 6.94% in the main sweep and 6.38% in confirmation; repeated API sum improves 8.80% and 7.85% | Useful for repeated negative-heavy batches; one-pass lifecycle remains uncertain; do not generalize to arbitrary nested memoization |

These are macOS arm64 host observations, including the JVM, for this exact
corpus. They are not Android device performance results. The prototypes were
measured separately; their percentages must not be added. Combined performance
was not measured and no combined speedup is claimed.

## Complete-workflow measurements

The main sweep has 12 balanced randomized AB/BA pairs per row; independent
confirmation has six. Each sample starts a fresh JVM. One pass means create,
all queries and close. Eleven passes mean the same lifecycle with a first pass
and ten repeats. All work, configuration, cache construction and cleanup is
charged. Values below are **median paired percentage changes**, followed by
exploratory paired-bootstrap 95% intervals. They are not ratios of cohort
medians. Negative changes favor the prototype.

| Hypothesis | Set | Passes | Lifecycle change [95% interval] | Peak physical footprint change [95% interval] |
| --- | --- | ---: | ---: | ---: |
| H1 | main | 1 | -3.59% [-5.47, -2.37] | -6.12% [-6.34, -5.96] |
| H1 | main | 11 | -0.41% [-1.59, +0.29] | -5.91% [-6.06, -5.72] |
| H1 | confirmation | 1 | -1.56% [-4.39, -0.03] | -5.94% [-6.13, -5.62] |
| H1 | confirmation | 11 | -2.73% [-4.82, -0.35] | -6.20% [-6.55, -6.06] |
| H2 | main | 1 | -2.35% [-3.97, +0.24] | -1.77% [-1.93, -1.46] |
| H2 | main | 11 | -0.36% [-1.31, +0.95] | -1.71% [-2.02, -1.53] |
| H2 | confirmation | 1 | -6.24% [-9.78, -1.05] | -1.59% [-2.06, -1.41] |
| H2 | confirmation | 11 | -2.94% [-3.43, -0.46] | -1.74% [-2.02, -1.33] |
| H3 | main | 1 | -1.48% [-5.11, -0.16] | -0.01% [-0.17, +0.25] |
| H3 | main | 11 | -6.94% [-8.94, -5.57] | -0.12% [-0.20, +0.10] |
| H3 | confirmation | 1 | -1.65% [-6.23, +0.35] | -0.03% [-0.46, +0.20] |
| H3 | confirmation | 11 | -6.38% [-6.76, -4.34] | +0.10% [-0.03, +0.23] |

A/A calibration of identical code exhibited a few percent of time drift.
H1/H2 main 11-pass lifecycle intervals include zero; faster confirmation does
not establish a universal or stable acceleration. H3's repeated-workflow gain
reproduces across cohorts. H3 close time changes direction between cohorts,
so no closing-speed benefit is attributed to it. RSS is retained as auxiliary
data because it was much noisier than physical footprint. All 216 final
measurement processes have valid probe endpoints, and their observed window
peak footprint equals the whole-process peak; no failed probe was interpreted
as a memory gain.

## Mechanisms, tradeoffs and counterexamples

H1 removes 123,755,360 bytes of otherwise unused logical slot storage on this
workload. Its final allocation is one numeric directory of 65,383 slots and
nine Ready payloads. Full directory coverage would reduce that advantage.

H2 halves the logical range/header storage to 31,200,372 bytes, but geometric
ID growth raises payload capacity by 874,352 bytes. It moves 11,435,868 ID bytes
during 696 growth allocations across 41 DEXes. It performs one decode pass and
preserves order, duplicates and empty rows. The remaining metadata, descriptors
and other relations dominate much of the memory and destruction cost.

H3 skips 2,232,889 of 4,143,768 parse attempts across the large batch and fallback
in a diagnostic pass. Its directories request 644,080 bytes cumulatively across
82 jobs; this sum is not simultaneous peak memory. The 1 MiB limit applies to
live requested bit-array payload per query. Positive and unknown strings are
parsed normally. Empty-string occurrence handling remains outside the bypass.
Queries and DEX jobs own separate memo instances.

In the local trie counterexamples, repeated negatives reduce 500,000 parses to
64, whereas unique negatives and repeated positives save none. The revised
stack-based microtest shows about 7% overhead on unique short negatives, 1.5%
on repeated positives and 9.4% on an oversized-directory fallback in this run.
These short local timings are illustrative, not QQ scores or portable bounds.
With a maximum-size directory and zero/one visits, setup costs about 8 microseconds
on this machine even though little work is performed. H3 allocates before scope
filtering, so sparse/no-candidate queries are an explicit limitation. The
prototype stops expansion when a directory cannot fit; it does not evict or
repeatedly rebuild entries.

## Verification and review

- All three prototypes pass 11 full fingerprint passes against the frozen
  baseline, including stage multiplicity, selected descriptors and control flow.
  All 216 measurement runs preserve counts and execution paths.
- H3 with a zero-byte memo budget passes the same 11-pass contract. It performs
  normal parsing rather than interpreting budget exhaustion as an empty result.
- Component builds pass native compilation, the JVM JAR and Android release
  AAR assembly. The latest full H3 run passes 71 JVM tests; H1/H2 and their
  combination were checked at their corresponding source states. Android
  runtime/performance testing is outside this host phase.
- Exhaustive native checks compare strings, opcodes and numbers over 60,457 demo
  method IDs, including 8,109 without code and 16 with repeated string uses.
  They cover same/different cold slots with eight readers, full-first reads,
  lazy-to-full transitions and retained lazy payloads. A separate JVM test
  races metadata readers with full warm-up.
- Sixteen rounds of eight jobs compete for a shared memo quota. Each holds its
  admitted object until all jobs report; live requested payload stays at
  524,296 bytes under the 1,048,576-byte budget, with 112 rejections in total.
  Destroying the objects restores the full quota. An injected nothrow allocation
  failure restores its quota while the failed object remains alive.
- Nine runner counterexample tests pass. Actual JDK identity, injected JVM
  options, missing reports/passes and invalid memory observations are guarded.
- Three iterative Pro reviews read actual pushed source. See `PRO-REVIEW.md`
  for scope, verified findings and the changes made in response. Consultation
  was not used as a substitute for measured results or component checks.

No API/schema semantics or default experiment settings were changed. The main
checkout was left outside this experiment; no merge or release was performed.
The existing unrelated native issues discussed before this phase are not claimed
as fixed. The QQ query definition superset is not an observed enabled-feature
profile, and three discovery workflows retain their baseline empty outcomes.

## Reproduction and data

Follow `README.md` for exact flags and commands. Inputs remain pinned to QQ
9.3.55 (41 DEXes) and QAuxiliary `01801ffd`; baseline `1d936bd`.
Artifact manifests bind the compiler/configuration, source state and native hash.
`evidence/all-samples.csv` contains every final measurement, and per-hypothesis
folders retain JSON samples, summaries, manifests and diagnostics.
`evidence/validation.json` records the final audit. Full reports, GC logs and
build logs are also packaged in the external `phase1-evidence.zip`; the APK and
native build trees are excluded from that archive. An archive checksum is kept
beside it. No slow sample was removed. The one rejected stale-verification
attempt and earlier calibration/diagnostic runs remain available for inspection.

This phase stops here. Any wider relation rewrite, paging, generic query DAG,
positive-result memo, persistent index or device benchmark is a separate scope.
