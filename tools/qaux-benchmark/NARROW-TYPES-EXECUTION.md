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
- [ ] Complete boundary, oracle, sanitizer, JVM and Android verification.
- [ ] Review the source delta and reconcile relevant findings.
- [ ] Record measured layout changes and a bounded performance comparison.

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
prototype passed 71 JVM tests and all four AAR ABIs. The compatible final source
must repeat the affected builds and boundary/oracle checks before timing.
