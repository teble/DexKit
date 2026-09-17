# Android optimization-level size comparison

This compares the current retained optimization combination plus uncached
member descriptors at four compiler optimization levels. The packaged Android
Release configuration was already `-Oz`. Only the effective C/C++ Release
optimization flag changes; full LTO remains enabled at all four levels.

## Packaged shared-library sizes

Sizes below are the stripped `jni/<abi>/libdexkit.so` members extracted from
the actual Gradle AARs. KiB means 1024 bytes. File size is not runtime RSS/PSS.

| ABI | O3 KiB | O2 KiB | Os KiB | Oz KiB | Oz versus O3 |
| --- | ---: | ---: | ---: | ---: | ---: |
| arm64-v8a | 535.84 | 494.91 | 439.41 | 391.57 | -26.92% |
| armeabi-v7a | 377.55 | 352.47 | 299.58 | 260.44 | -31.02% |
| x86 | 610.42 | 570.00 | 457.67 | 430.18 | -29.53% |
| x86_64 | 603.54 | 559.01 | 463.46 | 415.63 | -31.13% |

Exact standalone SO file bytes:

| ABI | O3 bytes | O2 bytes | Os bytes | Oz bytes |
| --- | ---: | ---: | ---: | ---: |
| arm64-v8a | 548696 | 506792 | 449960 | 400968 |
| armeabi-v7a | 386612 | 360932 | 306772 | 266692 |
| x86 | 625068 | 583676 | 468652 | 440508 |
| x86_64 | 618024 | 572424 | 474584 | 425608 |

Each SO relative to the existing Oz configuration:

| ABI | O3 versus Oz | O2 versus Oz | Os versus Oz |
| --- | ---: | ---: | ---: |
| arm64-v8a | +36.84% | +26.39% | +12.22% |
| armeabi-v7a | +44.97% | +35.34% | +15.03% |
| x86 | +41.90% | +32.50% | +6.39% |
| x86_64 | +45.21% | +34.50% | +11.51% |

## Selected ELF sections

These are section byte sizes, not a measurement of resident process memory.
The full section list and load segments are retained in the evidence.

| ABI | Level | .text KiB | .rodata KiB | .data + .data.rel.ro KiB | .bss KiB | Unwind tables KiB |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| arm64-v8a | O3 | 451.17 | 4.99 | 4.89 | 1.05 | 50.81 |
| arm64-v8a | O2 | 410.18 | 4.98 | 4.89 | 1.05 | 50.89 |
| arm64-v8a | Os | 343.29 | 4.84 | 4.89 | 1.05 | 62.41 |
| arm64-v8a | Oz | 257.06 | 4.85 | 4.95 | 1.04 | 100.54 |
| armeabi-v7a | O3 | 342.58 | 4.32 | 2.51 | 0.41 | 14.44 |
| armeabi-v7a | O2 | 317.09 | 4.32 | 2.51 | 0.41 | 14.85 |
| armeabi-v7a | Os | 260.05 | 4.27 | 2.51 | 0.41 | 19.05 |
| armeabi-v7a | Oz | 214.02 | 4.30 | 2.54 | 0.40 | 25.78 |
| x86 | O3 | 547.55 | 5.61 | 2.45 | 0.44 | 41.49 |
| x86 | O2 | 506.04 | 5.61 | 2.45 | 0.44 | 42.59 |
| x86 | Os | 383.34 | 5.33 | 2.45 | 0.44 | 53.24 |
| x86 | Oz | 332.18 | 5.46 | 2.48 | 0.43 | 76.64 |
| x86_64 | O3 | 520.61 | 5.74 | 4.90 | 1.04 | 48.62 |
| x86_64 | O2 | 474.27 | 5.71 | 4.90 | 1.04 | 50.46 |
| x86_64 | Os | 369.31 | 5.32 | 4.90 | 1.04 | 60.26 |
| x86_64 | Oz | 297.38 | 5.40 | 4.95 | 1.02 | 84.02 |

## Fixed inputs and checks

- Native engine: `2be1a63f1ac9de56f6e5cd53123322388e598006`; source snapshot: `2f2b1982591f1175832da3f104fb0c9ab6b29b30`.
- NDK `26.1.10909125`, minSdk 21, full LTO, section garbage collection,
  stripped symbols, the same C++ runtime, and 16 KiB load alignment.
- All sixteen retained experimental switches match the prior validated
  uncached build. Diagnostics and internal metrics remain disabled.
- All 16 libraries were built by `:dexkit-android:assembleRelease`.
  Every realized compile and link command was checked. After normalizing
  generated build-directory names, they differ only in the selected final
  optimization flag. Earlier NDK default `-O3` arguments are overridden
  by the final selected argument; checking the effective final flag avoids
  mistaking the earlier default for the active optimization level.
- All libraries are stripped ELF shared objects of the expected machine
  type, export the same 34 JNI entry points, retain the same dependencies
  per ABI, and preserve 16 KiB PT_LOAD alignment.
- Non-native AAR members are byte-identical across the four variants.
- Native Oz matches the previously saved baseline: `true`.
- Complete Oz AAR matches the previously saved baseline: `true`.
- The normal Gradle output is left at Oz; production defaults are unchanged.
- At the user's request, this round measures size only. No Android runtime
  timing or runtime correctness test was performed for these compiler
  variants. The results do not establish which level executes fastest.

## Artifacts and reproduction

- `O3`: [dexkit-android-O3-release.aar](/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/android-optimization-levels-v1/artifacts/O3/dexkit-android-O3-release.aar)
  SHA256 `81507525de4dfe2ddd78fa89f0869bd4c990a61796230293e9d876b9f248ea02`.
- `O2`: [dexkit-android-O2-release.aar](/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/android-optimization-levels-v1/artifacts/O2/dexkit-android-O2-release.aar)
  SHA256 `66992669a194bf02fabfdabdc10035ea84a5a3074ef64eb4028ca74ca05e9cdb`.
- `Os`: [dexkit-android-Os-release.aar](/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/android-optimization-levels-v1/artifacts/Os/dexkit-android-Os-release.aar)
  SHA256 `2c1483967d3c795f1bc973278cfa0bbfe358f35136e72539137385026bec8700`.
- `Oz`: [dexkit-android-Oz-release.aar](/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/android-optimization-levels-v1/artifacts/Oz/dexkit-android-Oz-release.aar)
  SHA256 `70f1727aabe443e4fde5b797b0dfa45af62d9caa5f9b017c26ebe49760709b79`.

Extracted SOs are alongside each AAR under `jni/<abi>/libdexkit.so`.
The [evidence directory](evidence/android-optimization-levels/v1/) records
the frozen source plan, exact Gradle commands, init script, CMake caches,
compile/link commands, ELF inspection output, hashes and size summaries.
Compiled AAR/SO files are local artifacts and are excluded from git.
