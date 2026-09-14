# Architecture experiment evidence

Scope, pinned inputs and acceptance rules are in `EXPERIMENT-PLAN.md` and
`README.md`. Dates below are 2026-09-15 Asia/Shanghai. All numbers are macOS arm64
host observations for this corpus, not Android performance claims.

## Baseline and calibration

The unchanged `1d936bd` engine matches the frozen output over 11 passes after
rebuilding with the immutable native builder. The returned batch key set was
added to the frozen contract from the original binary; all previous stage
multisets, selections and outcomes remained unchanged. Verification and timing
share workflow bodies. Eight runner counterexample tests pass.

The first fixed-heap A/A calibration uses the original Gradle binary SHA-256
`267e898c8ed039cc8cd3b8120709760defdf5d7bfb76a8692c0575a443bb19ea` (see the exact full hash
in `evidence/calibration/*samples.json`; sample data are authoritative).
Each workload has six balanced fresh-process pairs. All observations are kept.
The rebuilt baseline used for A/B is
`03eddd050d473950dbfc2d1757f1f541f11e168036b4431024c8c343ad5ce004`.

| Workload | A/A lifecycle median change | Exploratory 95% interval | Peak physical footprint median change | 95% interval |
| --- | ---: | ---: | ---: | ---: |
| 1 pass | +0.17% | -5.28% to +2.00% | -0.067% | -0.240% to +0.217% |
| 11 passes | +1.72% | +0.25% to +3.51% | +0.101% | -0.295% to +0.306% |

Time differences of a few percent need caution even if one bootstrap interval
excludes zero: identical binaries exhibited drift. RSS was much noisier than
physical footprint despite a fixed, pre-touched 256 MiB heap. Peak physical
footprint is the primary memory metric, RSS is auxiliary. Both include the JVM.
Post-close RSS retention is not a leak diagnosis; live malloc bytes fall much
further than RSS on this allocator.

## Selected hypotheses and predeclared tests

H1: allocate each per-method lazy-slot directory only when its feature is first
accessed in that DEX. Preserve stable slot/payload addresses and the existing
publication protocol; never retire a published lazy array after full warm-up.
Predict lower create and close costs and lower peak footprint for this sparse
workload. Contradiction: little saved resident memory or an offsetting first-use,
full-cache or concurrency regression. First prototype allocates a whole directory
on first use; paging is outside this bounded experiment.

H2: build only `method_using_string_ids` directly into contiguous IDs plus one
offset/length range per method. Preserve code order, duplicates and empty methods.
Do not build the old representation first. Predict fewer headers and allocations,
lower construction/destruction cost and footprint. Contradiction: append growth,
range lookup or decode overhead offsets the benefit. Other adjacency tables are
outside this first prototype.

H3 is conditional on attribution: reuse complete trie parse results for repeated
DEX string IDs within the same batch query and DEX, including negative results,
under a bounded budget. Cross-query memoization and generic query DAG memoization
are outside scope. Repeated visits alone do not justify implementation; the
avoidable cost must matter compared with lifecycle cost and cache overhead.

For each admitted prototype: full 11-pass equivalence, semantic regression checks,
12 balanced A/B pairs for 1 and 11 passes, then six new confirmation pairs if it
shows a useful improvement. Keep all runs. Diagnostic builds are excluded from
timing. The 10% lifecycle / 15% peak-memory guideline determines whether to expand
the architecture, not whether a smaller observation is real.

## Baseline diagnostic census

The diagnostic binary preserves all frozen results. Its pre-close census shows:

- Three lazy-slot arrays: 2,600,031 slots each, 16 bytes per slot, 119.02 MiB total.
  Only nine numeric slots are Ready; opcode and string lazy slots are unused.
- String-use storage: 59.51 MiB vector headers, 10.07 MiB payload capacity and
  552,475 nonempty per-method buffers. This directly motivates H2.
- Callers, invokes, field edges, descriptors and base metadata remain substantial;
  neither H1 nor H2 removes them. The census is not a complete allocator account.
- A 149-group batch visits 2,071,884 string uses but only 949,982 distinct DEX
  string IDs (54.15% repeated visits). It scans 39.19 MB including 24.69 MB unique
  bytes. Xorshift PRNG 1/1024 timing samples contain 0.421 ms total parse time and
  0.135 ms duplicate parse time summed across workers. This is attribution with
  instrumentation overhead, not a precise or rigorous speedup bound.
- The three-group fallback repeats the same visits, with 0.219 ms sampled parse
  time and 0.076 ms duplicate parse time. Negative parses and trie size matter.
- In this diagnostic run, close is 708 ms, worker shutdown 0.20 ms and the census
  itself 51.73 ms. Most close cost is outside worker shutdown; attribution of
  member destruction remains approximate.

Raw aggregate census and calibration samples are under `evidence/calibration/`.
Full run logs, artifact manifests and GC logs remain in the external data
directory referenced by each experiment run; the QQ APK is not committed.

## H3 admission after independent attribution

Three xorshift seeds (17, 5309, 9187), each mixed with the DEX ID, preserve all
frozen results. For the 149-group batch, 99.15%, 99.42% and 99.17% of samples
have empty raw trie results. Duplicate sampled parse sums are 0.137, 0.142 and
0.107 ms; empty-clock sums are 0.037, 0.039 and 0.035 ms across all samples.
Long strings are poorly covered (0--2 samples per run), so this is not a
precise time bound or evidence that positive-hit filtering is cheap.

This admits a narrower H3 prototype: memoize only completed empty `ParseText`
results, as one bit per string ID in a single batch/DEX job. Positive values are
recomputed normally. The query shares a 1 MiB cap on live requested bit-array
payload across workers. If a directory cannot fit, or allocation fails, that job
uses the original parse path. There is no eviction, partial entry, cross-query
reuse or generic nested-matcher memoization. Stack objects and allocator metadata
are outside this payload cap and must not be described as a whole-process cap.
Empty-string occurrence handling stays before the cache bypass.

Predicted benefit: skip repeated negative scans with very small state. A valid
negative finding is unchanged/slower lifecycle despite many fewer parses. Test
unique short negatives, repeated positives, an oversized directory and zero
budget separately; a cache that cannot fit bypasses from the start rather than
thrashing. Use the same 12 paired 1/11-pass A/B and six confirmation pairs if
promising. No new architecture beyond these three hypotheses is in this phase.

## H1 measured result

The 12-pair main sweep and independent six-pair confirmation both preserve all
counts and execution paths, following full 11-pass fingerprint verification.
The three directories fall from 124,801,488 logical bytes to 1,046,128 bytes: only
one DEX's 65,383 numeric slots is allocated, with nine Ready payloads.

| Workload / set | Lifecycle median change | Footprint median change | Close median change |
| --- | ---: | ---: | ---: |
| 1 pass / main | -3.59% | -6.12% | -8.53% |
| 11 passes / main | -0.41% | -5.91% | -7.79% |
| 1 pass / confirmation | -1.56% | -5.94% | -3.18% |
| 11 passes / confirmation | -2.73% | -6.20% | -14.25% |

Disposition: stable modest memory saving, variable small lifecycle saving. It
does not reach the expansion guideline. The 11-pass main lifecycle interval
includes zero; confirmation timing does not erase that limitation. All 48 main
reports have the same aarch64 JBR 17.0.6+10-b785.1 and valid probe endpoints.
After review, identity hashing and failed-probe guards were added outside the
lifecycle timer; confirmation reverified the same native binaries with the new
adapter. One attempted sweep stopped before its first JVM measurement because
its verification record was from the older adapter; that attempt is retained.

## H2 main result (confirmation pending)

The checked-range prototype preserves 11-pass fingerprints and all measured
counts/paths. Twelve pairs show -2.35% lifecycle / -1.77% footprint for one pass;
-0.36% lifecycle / -1.71% footprint for 11 passes. Both lifecycle intervals include
zero. String-use headers fall from 62,400,744 to 31,200,372 bytes; payload capacity
rises from 10,561,680 to 11,436,032 bytes. Live buffers fall from 552,516 to 123.
The direct append makes 696 growth allocations and moves 11,435,868 ID bytes
across 41 DEXes. It decodes each instruction once; no old representation is built.

H1 and H2 together pass 70 JVM tests and all Android release ABIs. A diagnostic
component test independently compares every demo method's strings/opcodes/numbers
between lazy and full representations: 60,457 method IDs, 8,109 without code, 16
with duplicate string references. Eight readers exercise same/different cold
slots; full-first getters allocate no lazy directory with H1 enabled, and old lazy
payloads remain valid after full warm-up. These checks are correctness evidence,
not performance samples.
