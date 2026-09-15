# Incremental optimization validation

Status: in progress. The user authorized individual validation of all five
ranked candidates and continued Pro collaboration on 2026-09-15.

## Baseline and decision rule

Use the measured H1/H2/H3 + structural cross-reference identity + descriptor
fast hits + raw interfaces combination (best5). Freeze fresh native artifacts
and their source/compiler manifests. Do not compare a new candidate only with
the old all-flags-off engine or add percentages from separate comparisons.
Keep all new experiment switches OFF by default.

Preserve the frozen QQ 9.3.55 results, multiplicity, selected descriptors and
dependent control flow. Host reflection is outside the acceptance boundary.
Use cross-DEX fixtures and adverse workloads for semantics that QQ does not
exercise. Correctness failures reject a candidate until fixed; they are never
reclassified as empty results or used to update the frozen expectations.

Measure complete create/query/repeat/close workloads and memory, including
deferred work, allocation growth, publication and cleanup. Diagnostics and
result hashing are separate from timed samples. Use fresh-process balanced
pairs, retain all valid samples and confirm promising results independently.
Memory improvements with a demonstrated time regression are not preferred.
An unresolved interval is reported as unresolved, not as zero regression.

## Ordered work

1. Field dependency split: count actual reverse-row consumers first; separate
   forward uses, field identity resolution and reverse adjacency readiness.
   Check forward-first then reverse/full-cache, full-first, concurrency,
   duplicates, ordering, cross-DEX resolution and nested logical matchers.
2. Dense atomic descriptor pointers: stable standard strings, bounded cold
   locking and acquire/release publication. Exercise sparse and high coverage,
   SSO/long strings, hot lookup, repeated output and retained views.
3. Contiguous forward invocation rows: direct append with checked lengths and
   borrowed ranges; preserve order and duplicate calls. Exercise huge rows,
   mixed density, caller construction and nested invocation matching.
4. Raw source-file metadata: recover views through existing ClassDef indexes;
   preserve absent/undefined metadata and cross-DEX behavior. Measure source
   matching and repeated class output, not just reduced array capacity.
5. Single-requirement invoke/caller matching: reuse the original ordered judge
   without allocating the general one-to-one solver. Preserve counts, Equal,
   empty/multiple requirements and nested logic. Exercise late hits and misses.

Each candidate receives its own evidence and keep/reject/unresolved decision.
Use best5 as the independent control; measure the final selected combination
directly against best5 after checking interactions. Do not silently expand the
phase into additional architectural rewrites.

## Review and completion

Continue the bound Pro conversation for meaningful design/source/result reviews:
https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

Pro reads actual fixed revisions through the selected GitHub connector. Check
its findings locally; its suggestions are not new authority or performance
evidence. Existing authorization permits commits and pushes only to the user's
teble/DexKit fork. Do not push to LuckyPray/DexKit.

Complete when all five candidates have evidence-based decisions, retained
changes pass native/JAR/JVM and Android packaging checks, and the final report
records measurements, rejected paths, remaining uncertainty and reproduction.
No new Goal, default enablement, upstream merge or release is implied.

## Progress

- Baseline source and previous measurements reviewed.
- Field diagnostics confirmed zero reverse-row consumption in eleven QQ passes.
- Field split and one bounded binding-filter revision are complete, including
  six lifecycle-order checks, independent byte oracles, ASan/UBSan, 71 JVM tests,
  four Android ABIs and revised paired timings. Keep it conditional: QQ wins,
  but reverse-consumer workloads still regress about 1.5--2%.
- Dense descriptor pointers are validated: sparse QQ peak memory improves,
  but full SSO output lifecycle regresses by about 12.5% for two repetitions
  and 2.0% for sixteen. Independent confirmation and native/JVM/AAR checks
  passed; exclude it from the generally preferred combination.
- Contiguous invocation rows pass complete ordered oracles, 24 matcher cases,
  five initialization sequences, ASan/UBSan, 71 JVM tests and four Android ABIs.
  Independent QQ gains reproduce, as do mixed/giant matcher regressions and
  mixed-row memory costs. Retain a conditional prototype, not a general win.
- Raw source metadata and single-requirement relation matching remain.
