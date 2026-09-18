# Narrow native index types

The user requested type-only memory reductions. Keep vector growth, reserve,
allocation order, row representation, traversal order and duplicate semantics
unchanged. Do not include exact forward allocation, shrinking, pointer-to-offset
views, raw metadata lookup changes, or class-field ranges in this increment.

## Scope

- Add default-OFF `DEXKIT_EXPERIMENT_NARROW_TYPES` and its Gradle property.
- Use checked 32-bit offsets for compact string, invocation, field and caller
  directories, including the temporary caller counts/write cursors.
- Store invocation targets and class-method/work-list IDs in 16 bits. Store
  caller and field reader/writer references as two 16-bit IDs (four bytes).
- Store class-definition indexes in 16 bits. Keep string IDs, row lengths,
  access flags, public metadata IDs and cross-reference identities unchanged.
- This opt-in representation requires each input DEX to contain at most 65536
  method IDs. Check that domain during base initialization, before narrowing.
  ID 65535 is valid; no ID value doubles as an empty marker. Check cumulative
  offsets/counts before arithmetic or casts, including cross-DEX aggregation.
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
- [ ] Complete boundary, oracle, sanitizer, JVM and Android verification.
- [ ] Review the source delta and reconcile relevant findings.
- [ ] Record measured layout changes and a bounded performance comparison.

The second development build passes the component boundary checks, the complete
caller contract oracle, the mixed invocation oracle, and field relation bytes.
The first build exposed one remaining class-method matcher template fixed to
uint32_t; it now follows the stored local method type. These are development
checks, not final verification or performance samples.
