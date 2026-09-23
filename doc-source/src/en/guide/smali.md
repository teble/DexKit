# Native smali output

The experimental `MethodData.toSmali()` and `ClassData.toSmali()` APIs generate
smali on demand from the result's original DEX. They use the existing bridge and
do not require a runtime smali/baksmali dependency. Opt in with
`@OptIn(DexKitExperimentalApi::class)` in Kotlin.

```kotlin
val method = bridge.getMethodData("Lexample/Target;->run()V")!!
val fragment: String = method.toSmali()
val completeClass: String = bridge.getClassData("Lexample/Target;")!!.toSmali()
```

A method result is a `.method` through `.end method` fragment. A class result
contains its declaration, source, interfaces, fields, initial values, methods
and annotations. A referenced but undefined class/method returns `NOT_DEFINED`;
the API does not redirect a result to a different DEX's same-named definition.
The fragment needs its original class context before assembly. Identity checks
cover one request; independently exported fragments do not share an identity map.

Both calls return independent strings. Closing the bridge does not invalidate
previous text, but another call on its result throws `SmaliException` with
`BRIDGE_CLOSED`. Requests share the bridge's query admission and lifecycle lock.
Temporary parsing/output state is released per request; whole-class output
releases each method plan before processing the next. Source memory borrowed by
a bridge must stay immutable during all operations.

## Options and errors

`org.luckypray.dexkit.smali.SmaliOptions` defaults to `SmaliDebugMode.NONE`.
This skips method debug parsing, including parameter names, line numbers and
local-variable events. It retains class `.source`, parameter annotations and
exception handlers. `STRICT` preserves parameter names and the interpreted debug
events (positions, locals, source changes, prologue and epilogue markers). Position
events require an instruction start; other events can also occur at method end.
Unrepresentable positions return `DEBUG_NOT_REPRESENTABLE`. Truncated encodings,
invalid references/registers, an end without a live local, and a restart without
an ended local return `MALFORMED_INPUT`. Implicit receiver/parameter locals are
initialized, including wide parameters. This is not full ART verification. No
initial line event is invented.

Output, input and item budgets accumulate over the complete request, including
all methods in a class. Code-unit and nesting limits apply per method and per
nested value respectively:

| Option | Default | Meaning |
| --- | ---: | --- |
| `maxOutputBytes` | 16 MiB | UTF-8 output bytes; also bounds temporary text fragments |
| `maxInputBytes` | 64 MiB | Cumulative bytes visited, including repeated references |
| `maxCodeUnits` | 1,048,576 | Maximum 16-bit code units in one method |
| `maxItems` | 1,048,576 | Cumulative record/plan work budget |
| `maxAnnotationDepth` | 64 | Maximum nested annotation/array depth; at most 256 |

These limits do not cap total process memory: output capacity, conversion buffers
and the Java String can coexist. Android builds disable C++ exceptions and do not
promise recovery from arbitrary allocator exhaustion.

`SmaliException` exposes stable error and phase codes, source DEX/member identity,
`containerByteOffset`, `codeUnitOffset`, and numeric detail. An unknown offset is
`-1`. A failed request returns no partial text and does not invalidate the bridge.
C++ `DexKit::GetMethodSmali` / `GetClassSmali` return `SmaliStatus` and leave the
caller's output string unchanged on failure. Native callers must synchronize
destruction with all operations on the same instance.

## Compatibility

The reader accepts ordinary little-endian DEX 035, 037, 038, 039, 040 and the
experimental 041 container layout. The 041 path uses physical container offsets,
including references beyond a logical DEX span. This is input compatibility, not
a promise to recreate the original container layout or DEX bytes.

Standard instructions include polymorphic/custom calls, method types and handles.
Round-trip tests use host-only `org.smali:smali:2.5.2`, API 28, and dexlib2 as an
independent reader. The first implementation has completed validation within
the declared profile; this is not exhaustive verification of every DEX variant,
instruction combination or runtime. All four Android ABIs build, and the R8/JNI
consumer runs on the host. Android-device execution and comparison over a large
real-world APK corpus have not been performed.

Unsupported input fails explicitly: CompactDex, odex/quickened or reserved
instructions, reverse-endian/unknown versions, link data, hidden-API metadata,
shared or orphan switch payloads, referenced equal-content method handles with
distinct source IDs (which smali would merge), extended names outside the supported unquoted
grammar (including supplementary Unicode names), and encoded NaN payload/sign bits that smali cannot preserve. Strings
preserve UTF-16 code units, including embedded NUL and isolated surrogates; names
use the supported BMP Unicode grammar. Checks cover the structures needed for emission,
not ART type-flow verification or hardening of the existing bridge loader.

For custom native builds, `-DDEXKIT_ENABLE_SMALI=OFF` removes the implementation;
the API then returns `UNSUPPORTED`. Gradle builds expose the same switch as
`-PenableSmali=false`. Size experiments are separate and must use
`DEXKIT_SMALI_SIZE_PROBE=none` for production builds.

Assembler caveat: [smali 2.5.2's default-value test](https://github.com/JesusFreke/smali/blob/v2.5.2/dexlib2/src/main/java/org/jf/dexlib2/util/EncodedValueUtils.java)
treats static `-0.0` as default zero. When it trims a default-value suffix, it can
discard the negative sign even if more default-valued fields follow. DexKit emits
the exact signed hexadecimal literal, but this input shape is outside the
unpatched assembler's end-to-end bit-preservation guarantee. That is a value
change, not merely a different DEX layout. Tests cover both native text and this
known oracle limitation; separate raw-bit round trips use a later nonzero field.
