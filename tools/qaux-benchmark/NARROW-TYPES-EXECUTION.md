# Narrow native index types

The user requested type-only memory reductions. Keep vector growth, reserve,
allocation order, row representation, traversal order and duplicate semantics
unchanged. Do not include exact forward allocation, shrinking, pointer-to-offset
views, raw metadata lookup changes, or class-field ranges in this increment.

## Scope

- Add default-OFF `DEXKIT_EXPERIMENT_NARROW_TYPES` and its Gradle property.
- Use checked 32-bit offsets for compact string, invocation, field and caller
  directories, including the temporary caller counts/write cursors.
- Store invocation instruction operands in 16 bits. Preserve 32-bit method
  identities in class-method/work lists, callers and field reader/writer rows.
- Store class-definition indexes in 16 bits. Keep string IDs, row lengths,
  access flags, public metadata IDs and cross-reference identities unchanged.
- The user explicitly selected compatibility with methods above ID 65535. Do
  not introduce a method-table limit. Class-definition indexes have a checked
  16-bit domain; cumulative cache offsets/counts have a checked 32-bit domain.
  No ID value doubles as an empty marker. Check counts before arithmetic or
  casts, including cross-DEX aggregation.
- Preserve the existing wide representation with the switch OFF. Do not enable
  unrelated packed-field/cross-reference switches in the comparison.

## Validation

Check upper ID boundaries, empty/nonempty rows, counts above 65535, overflow
rejection, retained views, duplicate/order preservation and cross-DEX merges.
Compare full existing metadata, invocation, caller, field, string and QQ oracles
against the same-source control. Run sanitizers, required Core/JAR/JVM tasks and
the four-ABI Android release build. Verify actual layout/capacity independently
of process memory; report any input-domain restriction and performance tradeoff.

## Progress

- [x] Implement the guarded storage types and update consumers/diagnostics.
- [x] Complete boundary, oracle, sanitizer, JVM and Android verification.
- [x] Review the source delta and reconcile relevant findings.
- [x] Record measured layout changes and a bounded performance comparison.

The second development build passes the component boundary checks, the complete
caller contract oracle, the mixed invocation oracle, and field relation bytes.
The first build exposed one remaining class-method matcher template fixed to
uint32_t; it now follows the stored local method type. These are development
checks, not final verification or performance samples.

The first prototype restricted method tables to 65536 entries. A real 65537-entry
fixture confirmed that the old Reader accepts the wider domain. The user chose
to preserve it, so the final candidate separates InvokeOperandId from the still
32-bit LocalMethodId and removes the new method-table check. Earlier prototype
artifacts/tests remain separate and are not the final comparison. The Android
prefab library also required a portable equivalent of std::in_range; the fixed
prototype passed 71 JVM tests and all four AAR ABIs. Those results were kept
separate; the compatible final source repeated the affected checks before timing.

The final compatible source c537b23 passed normal/diagnostic, independent
NARROW-only/all-OFF, ASan/UBSan and packed-field/compact-field combination
checks, frozen full-output oracles, both real width fixtures, QQ 1/11-pass
oracles, 71 JVM tests (zero skips), all four Android ABIs and the docs build.
Five actual compiler configurations confirm the intended sizes; the Android
boundary components were also compiled with the project's prefab C++ library.
These are Android compilation/layout checks, not device runtime measurements.

Final QQ census saves 66,208,612 bytes (63.14 MiB), with identical logical
caller rows, member rows, growth counts and element capacities. One long-field
dump failed its final output-write check while the disk was full; lossless
compression of this task's old prototype logs/oracles released space. The same
binary and input then passed. Both the failed output and successful retry are
retained; no production code changed for that environmental failure.

Freeze a finite comparison before collecting performance: same-source control
versus NARROW_TYPES, with the retained Small14/raw/uncached combination unchanged.
Use six relation cases (match, duplicate match, large output, late/cold/full
caller construction) plus QQ p1/w4, p11/w4, p1/w1 and p1/w4 with final field
reverse access. Main and independently seeded confirmation each use six
balanced AB/BA pairs: 20 sweeps, 240 fresh processes. Keep adverse samples and
measure complete lifecycle plus physical peak. No builds, correctness checks,
profiling, archive compression or further tuning during timing.

All 20 sweeps / 240 fresh processes completed, with frozen inputs unchanged.
QQ physical peaks decrease 4.02--4.77% across both batches and all four cases.
Mixed invocation matching and single-thread/single-pass QQ have a repeated
lifecycle benefit; other lifecycle comparisons remain unresolved, with no
regression supported in both batches. See the complete
[results](NARROW-TYPES-RESULTS.md) and
[evidence](evidence/narrow-types/v1/manifest.json).
