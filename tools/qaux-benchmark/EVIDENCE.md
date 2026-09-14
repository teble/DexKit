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
  bytes. Systematic 1/1024 timing samples contain 0.421 ms total parse time and
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
