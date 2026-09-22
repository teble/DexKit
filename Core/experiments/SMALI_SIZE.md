# Initial smali body experiment

This is a small instruction-body experiment, not a complete disassembler or a
claim about the final feature's cost. It does not compare Reader/class traversal,
annotations, exceptions, modern references or debug information. Both paths
receive the same checked method instruction array, emit the same method header
and support the same seven opcodes: nop, move, const/4, const/16, goto, if-eqz and
return-void. The CodeIr path creates temporary DEX IR nodes for that body; the
lightweight path calls slicer's decoder directly. An exported C probe keeps each
path reachable through LTO and section GC. Normal builds exclude the probe.

Measured on 2026-09-23 from base `34ca4b8` plus this experiment:

| Android arm64-v8a build | stripped bytes | delta from B0 |
| --- | ---: | ---: |
| B0, probe disabled | 394896 | 0 |
| B1, CodeIr + Visitor | 435744 | 40848 |
| B2, slicer decoder + offsets | 412632 | 17736 |

| ELF section | B0 | B1 | B2 |
| --- | ---: | ---: | ---: |
| .text | 258720 | 274140 | 261576 |
| .rodata | 5112 | 11720 | 11032 |
| .data.rel.ro | 5000 | 8224 | 7048 |
| .rela.dyn | 11952 | 20424 | 18096 |
| .eh_frame | 76908 | 82164 | 77388 |
| .eh_frame_hdr | 23868 | 25580 | 24004 |

All three used NDK 26.1.10909125, API 21, Rikka cxx 1.2.0, `-Oz`, full LTO,
hidden visibility, no exceptions/RTTI, section GC and stripping, with metrics
disabled. Build the normal Android release first, then configure its arm64 CMake
build directory with `-DDEXKIT_SMALI_SIZE_PROBE=none`, `ir`, or `light` and build
target `dexkit`. Compare the resulting `libdexkit.so`, not the static archive.
For subsequent experiments, use a separate build directory with the same NDK,
flags and prefab configuration. If an existing build directory is used, reset
the cached selection to `none` explicitly before packaging and verify that
`DexKitSmaliSizeProbe` is absent from the dynamic exports. Current desktop and Android Gradle commands explicitly reset the selection to
`none`; standalone CMake users must select it themselves.
The original Gradle baseline before CMake reconfiguration was 394888 bytes;
the table uses the consistently reconfigured B0 for both deltas.

Host output comparisons passed for straight-line code, both branch shapes and
empty-body return; both paths rejected unsupported, truncated and invalid-register
inputs. On macOS arm64, `-Os`, 200 warm calls over 5000 const/4 instructions plus
return produced identical 70068-byte strings. Observed means were 1025 us with
CodeIr and 205 us with direct decoding, including the common preflight and text
copy (no JNI). This synthetic case is not a latency forecast for APK methods.
No allocation peak was measured in this first experiment.

The host driver is `smali_probe_driver.py`. For the original S0 comparison use
commit `d7429c8` in an isolated checkout and configure two separate directories:

```sh
cmake -S dexkit/src/main/cpp -B /tmp/smali-probe-ir -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DDEXKIT_SMALI_SIZE_PROBE=ir \
  -DCMAKE_CXX_FLAGS_RELEASE='-Os -DNDEBUG -fno-rtti -fno-exceptions'
cmake --build /tmp/smali-probe-ir --target dexkit
# Repeat into /tmp/smali-probe-light with DEXKIT_SMALI_SIZE_PROBE=light.
python3 Core/experiments/smali_probe_driver.py \
  --ir /tmp/smali-probe-ir/libdexkit.dylib \
  --light /tmp/smali-probe-light/libdexkit.dylib
```

Use the platform's shared-library suffix and set JAVA_HOME to a matching host
JDK for JNI discovery. The driver checks output equality/refusals before timing.

The body linkage result favors proceeding with the lightweight path, subject to
checking its full control-flow/metadata implementation and final Android sizes.
The full feature's enabled/disabled builds must be measured again for every ABI.

The baseline AAR built at the start of the task contained: arm64-v8a 394888,
armeabi-v7a 262908, x86 433444, x86_64 418872 bytes. Preserve compiler/configuration
identity when comparing these with final builds.

## First production API checkpoint (Debug.None)

The initial complete method/class path, including bounded reading, metadata,
modern references, JNI UTF conversion and error construction, built as follows:

| ABI | stripped bytes | delta from original baseline |
| --- | ---: | ---: |
| arm64-v8a | 447776 | 52888 |
| armeabi-v7a | 300004 | 37096 |
| x86 | 489388 | 55944 |
| x86_64 | 478696 | 59824 |

This includes the real `nativeGetSmali` endpoint and excludes the probe export.
All 122 JVM tests passed against the rebuilt host library, including independent
semantic assembly checks. Strict debug, further size tuning and final enabled/
disabled measurements are still pending; these numbers are a development
checkpoint, not the final feature budget.

## S1 control-flow extension

The final bounded extension has three closed, constant inputs: one packed-switch,
one width-one array payload and one catch-all. `DexKitSmaliControlProbe` selects
one fixture; other selections fail. Both backends emit identical method text.
The CodeIr path actually visits payload and try nodes. This compares lifting and
emission only; it does not add a general input verifier or a second production
metadata/debug writer. S0's arbitrary-input preflight remains unchanged.

With the compact opcode-name table and production smali disabled in all three
variants, isolated Android arm64 builds using the same NDK/flags measured:

| Variant | stripped bytes | delta from OFF |
| --- | ---: | ---: |
| OFF, public API stubs only | 396408 | 0 |
| CodeIr, S0 + S1 | 432608 | 36200 |
| Direct decoder, S0 + S1 | 407744 | 11336 |

Unlike original S0, the OFF baseline now includes the integrated public API's
unsupported stubs. Do not subtract across these two experiments. No comparative
allocation peak is claimed. The production writer's retained-memory checks are
recorded in `../SMALI_PLAN.md`. The probe stops at this subset.

To reproduce isolated ABI measurements from a configured Gradle build:

```sh
python3 Core/experiments/smali_measure.py \
  --cache dexkit-android/.cxx/Release/<configuration>/arm64-v8a/CMakeCache.txt \
  --output /tmp/dexkit-smali-size --modes off ir light
```

Use `--modes off on` for the complete production feature and repeat with each
ABI's cache. The script copies the effective compiler/toolchain/prefab settings,
builds outside Gradle output directories, records the source SHA and dirty state,
saves linker maps and section sizes, and checks probe exports. For a release
comparison the recorded source tree must be clean.

The host driver accepts `--control` to check S1 output equality. At the S1
checkpoint, S0's 5000-constant body still produced identical 70068-byte text;
observed means were 950 us with CodeIr and 205 us with direct decoding. This
remains a synthetic host observation, not a production workload benchmark.

## Complete production feature: final paired comparison

Implementation SHA: `e3e9ee0841ae3d2b7ee0f61278a95c97fbe88076`. All eight isolated
builds recorded this clean source SHA, used the configuration above and disabled
metrics/probes. ON includes Strict and all production entry points; OFF retains
the API's Unsupported stubs. The JSON record is `SMALI_SIZE_RESULTS.json`.

| ABI | isolated OFF bytes | isolated ON bytes | feature delta | Gradle packaged bytes | packaged delta from master |
| --- | ---: | ---: | ---: | ---: | ---: |
| arm64-v8a | 396408 | 446040 | 49632 | 446032 | 51144 |
| armeabi-v7a | 264008 | 301728 | 37720 | 301716 | 38808 |
| x86 | 434924 | 493892 | 58968 | 493884 | 60440 |
| x86_64 | 420472 | 477824 | 57352 | 477816 | 58944 |

The isolated ON files were 8-12 bytes larger than this Gradle packaging run.
Use the paired isolated columns for the feature delta, and the two packaging
columns for product size; do not mix these baselines. The arm64 feature delta is
49,632 bytes (48.47 KiB). The optional build switch removes those production
bodies without removing the public error-returning API. Neither variant exports
the experimental probes. The AAR has all four ABIs and no smali JVM dependency.

Selected ELF section deltas (ON minus OFF; bytes):

| ABI | .text | .rodata | .data.rel.ro | .rela.dyn | .rel.dyn | .eh_frame | .eh_frame_hdr | .ARM.exidx | .ARM.extab |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| arm64-v8a | 34124 | 7584 | 552 | 744 | 0 | 5004 | 1544 | 0 | 0 |
| armeabi-v7a | 28452 | 7136 | 276 | 0 | 248 | 0 | 0 | 688 | 860 |
| x86 | 46364 | 7920 | 276 | 0 | 248 | 3392 | 696 | 0 | 0 |
| x86_64 | 40138 | 8048 | 576 | 744 | 0 | 7088 | 664 | 0 | 0 |

File-size deltas also include section alignment and metadata. The name-table
change removes one pointer/relocation per opcode and is checked against all 256
source spellings. We do not attribute the complete feature delta to that one
optimization. Linker maps and raw section listings are generated alongside each
external measurement build; they are not production or committed build outputs.
