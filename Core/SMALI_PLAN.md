# Native smali output

Base: `34ca4b84814159aeb8e00c6634e85a4d624f3df6` (local master).
Delivery branch: `teble/DexKit:smali`.

## Scope

Add on-demand DEX-to-smali output for a defined method (a `.method` fragment)
and for a complete defined class. Reuse slicer's DEX structures and decoding;
do not add a runtime JVM disassembler dependency. Preserve source DEX identity,
annotations, encoded values, instruction operands, control flow and handlers.
The returned text owns its contents and survives bridge closure. Per-call state
is released after success or failure, with no persistent disassembly cache.

Debug information has an explicit policy. Omitting debug must skip its parsing,
not merely its printing. Unsupported formats and unrepresentable metadata must
produce a diagnostic, never successful partial output. This does not promise
that the existing DexKit loader validates arbitrary hostile DEX input.

## Plan and acceptance

- [ ] Establish the supported DEX/smali profile, bounded read/output contracts
      and baseline stripped Android library sizes.
- [ ] Compare a reachable CodeIr path and a lightweight slicer decoding path
      on the same small supported subset; record actual size and allocation
      tradeoffs before choosing the production body writer.
- [ ] Implement bounded method selection, text emission, reference/annotation
      handling, payload/try validation and explicit error results.
- [ ] Implement complete class output and Core/JNI/Kotlin entry points under
      existing query and bridge lifecycle protection.
- [ ] Test assembly and normalized semantics with pinned host-only smali/dexlib2;
      cover failure isolation, limits, Unicode, metadata, modern opcodes,
      nonzero container headers, concurrency, closure and temporary memory.
- [ ] Run native checks, JVM build/tests, Android release builds and documentation
      checks. Record stripped `.so` changes for all packaged ABIs.
- [ ] Submit actual pushed commits to the bound Pro review conversation, verify
      findings locally and repeat review after material corrections.
- [ ] Finish documentation and push the verified implementation to `teble:smali`.

## Review notes

The previous design review found bounds-check, 45cc register-list and positional
parameter-annotation bugs; the base commit above contains their fixes. The
existing Reader and CodeIr still have fail-fast paths, incomplete modern-index
support and debug merging assumptions. A successful bridge load is not evidence
that subsequently visited code or metadata is safe to lift through those paths.

An implementation choice remains open: checked input plus temporary CodeIr and a
single method-body Visitor, or checked input plus `dex::DecodeInstruction` and a
small offset-based emission plan. Both reuse slicer. Measurements and the amount
of duplicate validation needed will determine the choice; no size saving is
claimed before measuring the final linked library.

Method requests must not parse unrelated bodies/annotations in the same class.
Class requests must include every member or fail. Strings require MUTF-8-aware
escaping; descriptor/name grammar and JNI encoding need separate handling.
DEX 041 offsets refer to the physical container and may reach beyond the logical
DEX span. Resource limits are checked before growing temporary structures.

Status: planning and baseline measurements; no smali implementation yet.
