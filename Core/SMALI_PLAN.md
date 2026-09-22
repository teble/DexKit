# Native smali output

Base: `34ca4b84814159aeb8e00c6634e85a4d624f3df6` (local master).
Delivery branch: `teble/DexKit:smali`.

## Scope and architecture

On-demand DEX-to-smali for a defined method fragment or a complete defined class.
The production path uses checked input, slicer `DecodeInstruction`, and a small
plan keyed by code offsets. It does not invoke the old Reader/CodeIr fail-fast
paths or add a JVM disassembler runtime dependency. The bounded CodeIr comparison
stops at S0/S1; measurements are in `experiments/SMALI_SIZE.md`.

Core/JNI/Kotlin entry points retain source ownership, one query admission per
request and the bridge read lock through JNI conversion. Source memory must stay
immutable. Native callers synchronize destruction. Output owns its contents,
survives bridge closure and replaces the C++ destination only on success. Each
class member's code/debug plan is released before the next member; all request
state, including the handle registry, is released on success and failure.

None skips method debug parsing. Strict preserves interpreted debug events and
parameter names without synthetic initial positions. It checks encoding,
references, position boundaries and local end/restart prerequisites, including
implicit receiver and wide-parameter locals. This is not full ART verification.
Class source, parameter annotations and handlers are retained in both modes.

The bilingual smali guides specify supported versions, budgets, typed failures
and unsupported inputs. In particular, 041 uses physical container offsets;
source IDs remain owner-bound; class requests include every defined member or
fail; independent method requests do not share an identity registry. Native text
preserves negative zero, while the pinned assembler can trim its default-value
suffix. Supplementary Unicode names are explicitly unsupported because the
pinned lexer cannot assemble them; string literals preserve all UTF-16 units.

## Acceptance progress

- [x] Establish bounded reading, explicit compatibility/diagnostics and baseline
      stripped Android sizes.
- [x] Compare bounded CodeIr and direct-decoder bodies, record size/latency, and
      choose the production path without maintaining two full writers.
- [x] Implement method/class output, annotations, encoded values, modern
      references, control flow, payloads, handlers and Strict debug.
- [x] Integrate Core/JNI/Kotlin with lifecycle/query protection and consumer rules.
- [x] Check assembly semantics, limits, Unicode, numerical/register boundaries,
      reference identity, 041/shared data, concurrency, closure and memory.
- [x] Run native, full JVM, Android release and R8 consumer checks.
- [ ] Record final clean-SHA ON/OFF stripped sizes for every packaged ABI.
- [ ] Finish documentation build and the final fixed-SHA Pro review, then push
      the verified implementation and reports to `teble:smali`.

## Verification

Latest implementation checks: 137 JVM tests across 14 suites, no failures or
skips; native reader/selector checks include all 256 opcode names. Core/JVM
`cmakeBuild`, `jar`, `test`, Android `assembleRelease` and `smaliR8Consumer` pass.
The R8 check uses classfile output on the host and the Android consumer rules;
it executes real JNI success, typed native error and closed-bridge paths. It is
not an Android-device runtime test. A separately built feature-OFF host library
returns Unsupported through the same shrunk consumer.

Four assembled native seeds cover metadata, modern references, control flow,
numeric/register extremes and debug. The direct writer harness makes 111,676
calls: all-byte perturbations, whole-file truncations, and repeated success plus
late output-limit/deep debug-signature truncation failures. Normal and ASan/UBSan
runs pass; failed output remains unchanged. After warmup, macOS allocator live
bytes were unchanged for each seed. Whole-file truncations generally reject at
the header; the dedicated signature case reaches a missing LEB byte after an
event was allocated. Mutation successes are not semantic-oracle proofs.

Reproduce host/native checks after `SmaliOutputTest` exports its DEX fixtures:

```sh
bash ./gradlew :dexkit:cmakeBuild :dexkit:jar :dexkit:test \
  :dexkit:smaliR8Consumer :dexkit-android:assembleRelease
cmake -S Core -B /tmp/dexkit-smali-native -G Ninja \
  -DDEXKIT_BUILD_SMALI_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build /tmp/dexkit-smali-native -j 4
ctest --test-dir /tmp/dexkit-smali-native --output-on-failure
/tmp/dexkit-smali-native/dexkit_smali_writer_harness dexkit/build/smali-fixtures/*.dex
```

## Pro review record

The baseline includes prior slicer bounds, 45cc and positional annotation fixes.
Review of `d7429c8` led to `2620b42`: complete map ranges, reused empty class
results, method grouping, bounded temporary fragments and diagnostic context.
Review of `58ef5fa` led to `34f7724`: per-request handle identity checks, static
initial-value categories, annotation-directory member binding and switch offsets.

Review of `34f7724` confirmed those corrections and Strict's main event semantics.
It identified local end/restart preconditions and specific harness coverage gaps.
The current increment initializes/checks local states, tests unknown locals and
implicit parameters, exercises deep truncation and late failures, and separately
records the negative-zero oracle limitation. Pro reviews are source reviews;
local test/build results are independently executed, not attributed to Pro.
