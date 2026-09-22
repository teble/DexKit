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

The first implementation-planning review read `161590b` and the baseline Core,
JNI and Kotlin call paths. Adopt its single-admission rule: public method/class
entry points enter query execution once; internal member emitters never enter it
again (nested guards can deadlock at a concurrency limit of one or with warmup).
Class output uses one cumulative budget and releases each method plan in turn.
Source bytes must remain immutable throughout a call, including borrowed input.
Status values have explicit stable numeric IDs and identify the failing member.
Method debug suppression does not suppress class `.source` or parameter annotations.
Hidden-API metadata, unrepresentable names/values and unsupported variants require
explicit rejection. The complete supported profile is still under implementation.

Status: baseline Android build and initial body linkage experiment complete (see
`experiments/SMALI_SIZE.md`). A bounded reader and text/encoding primitives are
being implemented and tested with ASan/UBSan. No public smali API is connected yet.
The full Reader-versus-lightweight end-to-end comparison is deliberately not
claimed by the body-only experiment. Production is provisionally moving toward
direct slicer decoding to keep the new checked boundary independent of the old
Reader/CodeIr fail-fast and debug-merging paths.

The first source review read all of `d7429c8`'s reader, selector, tests and probe.
Confirmed corrections: check the entire map list against the data range; replace
reused class-member results even for empty classes; validate direct/virtual
grouping. Also bound temporary UTF-8/reference fragments, snapshot options,
preserve diagnostic context and make generic read helpers self-contained.
These are being covered by dedicated regression fixtures. The next experiment
extension is a bounded packed-switch/array-data/catch-all subset, after which the
CodeIr experiment stops growing. The production writer is checked independently
with host smali/dexlib2 rather than building a second complete implementation.
