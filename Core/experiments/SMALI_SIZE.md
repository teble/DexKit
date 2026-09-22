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
`DexKitSmaliSizeProbe` is absent from the dynamic exports. A normal Gradle command
does not reset an existing CMake cache selection.
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
