# QAuxiliary query replay on QQ 9.3.55

This is a host-side compatibility corpus for evaluating whether real QAuxiliary
query workflows are suitable for DexKit architecture experiments. It contains
switchable engine experiments and diagnostics. It is a macOS arm64 host benchmark;
it does not establish Android device performance.

The agreed experiment scope uses query-result equivalence as its regression
contract. Host reflection, hook installation and full QAuxiliary initialization
are not required acceptance gates. See `EXPERIMENT-PLAN.md` for the first
research phase and stopping criteria, `RESULTS.md` for its decisions, and
`EVIDENCE.md` for detailed observations and review history.

The [memory-layout results](MEMORY-LAYOUT-RESULTS.md) report the completed
cross-reference, field-token and growing descriptor-cache comparisons, including
their adverse cases. The user-requested
[sparse/dense descriptor follow-up](HYBRID-DESCRIPTORS-EXECUTION.md) has its own
design and validation boundary; its pending results do not replace that evidence.

## Pinned inputs

- QAuxiliary: `01801ffd013c95781dd360704adf48dc42ee8aa6` (2026-09-13).
- Initial DexKit baseline: `1d936bd9efa38e00b7336ef112e73ba4048324e6`.
- QQ: package `com.tencent.mobileqq`, version `9.3.55`, versionCode `15900`.
- APK SHA-256: `851242d139bb01ed8c787eadc30d7ec391437de550c697b5f4c65c32ec84286f`.
- APK URL: <https://downv6.qq.com/qqweb/QQ_1/android_apk/9.3.55_226abb86565ab9e9.apk>.

The APK, extracted target data, compiled replay classes and result files live
outside this worktree. They are not inputs to ordinary Gradle tests and are not
included in the library or demo APK.

## Extraction and replay

`extract.py` parses literal `UsingStr` and `UsingStringVector` definitions from
the pinned QAuxiliary checkout. It removes comments, preserves escaped strings,
rejects expressions it cannot interpret, and writes source references for each
target. `UsingStr` alternatives become separate groups; each string vector
remains an AND group. The engine receives `StringMatchType.SimilarRegex`, just
as QAuxiliary's backend does.

`QueryReplay.java` adapts five specialized finders and five feature discovery
workflows. It preserves query dependencies, conditional fallbacks, cardinality
checks, duplicate class results, and Java/Kotlin-side selection. In particular,
`findMethod(...).firstOrNull()` stays a complete query followed by selection; it
is not replaced with the native `findFirst` option.

`run.py` pins the APK and QAux revision, compiles the adapter against the selected
DexKit build, and records native/JAR/input hashes. Repeated passes compare result
multisets and selected descriptors. Non-diagnostic profiles also compare every
pass and execution sequence with `baseline/expected.json`, frozen from the
original engine. The corpus is available under `baseline/` for reproducibility.
Verification materializes and hashes descriptors; measurement keeps only counts,
selected descriptors and control-flow metadata. A measurement requires a successful
full verification of the same binary, adapter, dependencies, profile and at least
as many passes. Missing reports, missing passes and changed returned key sets fail.
Output reports and native snapshots are never overwritten.

Example on this machine, from `/Users/teble/project/android/DexKit-qaux-benchmark`:

```sh
env JAVA_HOME=/Users/teble/Library/Java/JavaVirtualMachines/jbr-17.0.6/Contents/Home \
    ANDROID_HOME=/Users/teble/Library/Android/sdk \
    bash gradlew :dexkit:cmakeBuild :dexkit:jar :dexkit:test --max-workers=2

python3 tools/qaux-benchmark/extract.py \
    --qaux-root /Users/teble/project/android/QAuxiliary \
    --output /Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/qaux-extracted

python3 tools/qaux-benchmark/run.py \
    --dexkit-root /Users/teble/project/android/DexKit-qaux-benchmark \
    --corpus /Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/qaux-extracted \
    --apk /Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/qq-9.3.55.apk \
    --output /Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/replay-next \
    --java-home /Users/teble/Library/Java/JavaVirtualMachines/jbr-17.0.6/Contents/Home \
    --threads 4 --passes 2 --profile all
```

Use a native JDK matching the native library architecture. The example uses an
arm64 JDK; this machine's default `java` is x86_64. The wrapper is invoked through
`bash` because its checked-out file is not executable.

Profiles:

- `all`: one batch of all literal targets followed by ten discovery workflows.
- `chains`: the discovery workflows without the preceding string batch.
- `batch`: raw candidates for all literal targets.
- `diagnostics`: deliberately relaxed probes for understanding current misses;
  these are not the original feature queries and are excluded from `all`.

The initial runner resolves Kotlin 1.9.20 and FlatBuffers 23.5.26 from the Gradle
cache, matching this DexKit baseline. Dependency changes require updating it.

## Interpretation boundaries

- The 139 literal targets are a source-derived superset, including old-version
  and TIM targets. They are not an observed set of enabled features on a device.
- Raw string matches are candidates. QAuxiliary's subsequent descriptor filters,
  host reflection, cardinality checks and cache writes are not reproduced for
  these 139 targets. A raw hit is not a successful feature initialization.
- Five specialized finders use the same query shapes and dependencies. Other
  feature discovery scopes and exclusions are recorded in `report.json`.
- `HideMiniAppPullEntry` uses DEX metadata to resolve its direct Conversation
  class names instead of QAuxiliary's ClassLoader/synthetic-class fallback.
- `AutoReceiveOriginalPhoto_cache_miss` selects the source's absent-class-cache
  path. Class loading is excluded. Its first query currently misses, so its
  later three steps are not covered on this APK.
- These samples provide real nested caller/invoke/field constraints and serial
  dependencies. They do not cover every deep Boolean/matching edge case; keep
  the small controlled demo and adversarial correctness fixtures.
- Repeated passes use the same bridge. Later passes repeat engine work with warmed native
  state; it does not simulate QAuxiliary startup with a persistent descriptor
  cache, which can skip the queries completely.
- Stages share caches warmed by earlier stages. Starting a new process does not
  guarantee cold filesystem pages. `chains` helps separate batch-induced warming.
- API timings include query construction, JNI and returned result objects.
  Descriptor materialization is recorded separately in verification. Formal timing
  uses `--mode measure`, includes required selection/filtering and result objects,
  and excludes hashes and per-stage logging. Create-to-close includes thread
  configuration, all queries, cache construction and destruction. Process peaks
  include the fixed JVM heap and native allocations, not just DexKit.

For a performance comparison, keep this selected corpus fixed and preserve its
dependency graph and cache state. A recorded enabled-feature profile can improve
representativeness later; it is not a prerequisite for this controlled engine
benchmark. Device runs are needed before making device-specific performance
claims. Keep current misses as explicit compatibility cases; do not silently
relax the original queries to improve the success rate.

## Formal native experiments

`build_native.py` builds a separate immutable `libdexkit.dylib`, CMake cache,
compiler/configuration manifest and source patch. New native source files are
also copied into the artifact. Formal measurements require the manifest to match
the library hash, with diagnostics and internal metrics compiled out. Baseline
and candidate use this builder with identical compiler settings; toggles differ.
The benchmark library is selected explicitly and does not replace Gradle outputs.

Use the same native arm64 JDK as above:

```sh
python3 tools/qaux-benchmark/build_native.py \
  --source-root /path/to/experiment-worktree --output /path/to/artifacts/base \
  --java-home /path/to/arm64-jdk17
```

The optional `memory_probe.cpp` JNI library records current RSS, process malloc
statistics, current physical footprint and lifetime peaks before create and after
close. On macOS it links against `libproc`; compile with the JDK `include` and
`include/darwin` directories. Pass its absolute path as `--memory-probe` to both
verification and measurement. The probe runs outside the lifecycle timer. A
window peak is available only if the process lifetime peak increased after the
pre-create snapshot. Whole-process `/usr/bin/time -l` peaks are also retained.

For example (use an empty output directory):

```sh
BENCH_JDK=/path/to/arm64-jdk17
clang++ -std=c++20 -O2 -dynamiclib \
  -I"$BENCH_JDK/include" -I"$BENCH_JDK/include/darwin" \
  tools/qaux-benchmark/memory_probe.cpp -lproc \
  -install_name @rpath/libqauxbench_probe.dylib \
  -o /path/to/artifacts/probe/libqauxbench_probe.dylib
```

Formal measurement requires an explicit JDK path and hashes its executable,
release metadata and JVM library. Unset `JAVA_TOOL_OPTIONS`, `JDK_JAVA_OPTIONS`
and `_JAVA_OPTIONS`; implicit extra JVM parameters are rejected. Probe failure
is missing data, and both endpoints must be valid for a window-peak inference.

Formal runs use `-Xms256m -Xmx256m -XX:+AlwaysPreTouch` and record GC logs. Use
`--mode verify --passes 11 --native-library /path/to/artifacts/base/libdexkit.dylib`
with the `run.py` arguments above. Repeat for the candidate. Then use:

```sh
python3 tools/qaux-benchmark/sweep.py \
  --dexkit-root /path/to/experiment-worktree --corpus /path/to/frozen-corpus \
  --apk /path/to/qq-9.3.55.apk --java-home /path/to/arm64-jdk17 \
  --variant base /path/to/artifacts/base/libdexkit.dylib /path/to/verify-base \
  --variant candidate /path/to/artifacts/candidate/libdexkit.dylib /path/to/verify-candidate \
  --output /path/to/new-sweep --pairs 12 --passes 1 --profile all
```

Run the predeclared 12 pairs for both 1 pass (create/query/close) and 11 passes
(first query plus 10 repeats), then six new confirmation pairs if promising.
Every sample starts a fresh JVM. Balanced randomized AB/BA ordering and all raw
samples are retained; no filesystem-cache flushing or sample deletion occurs.
First, repeated-sum and complete lifecycle times are separate. Passes in one
process are dependent, and pass 2 is not assumed to be steady state. Bootstrap
intervals describe paired medians and are exploratory, not a substitute for the
independent confirmation. `EVIDENCE.md` records calibration and decisions.

`DEXKIT_BENCHMARK_DIAGNOSTICS=ON` is exclusively for attribution. Its slot/cache
capacity and sampled string-scan measurements perturb the workload and are never
used as formal performance samples. The cache census counts logical capacity and
known buffers, not allocator overhead or every native allocation.

## Prototype switches and component checks

All switches default to OFF. Enable exactly one for an isolated A/B comparison.
Use the recorded `engine_commit` and CMake settings in each artifact manifest
to reproduce the measured snapshot, rather than assuming the latest HEAD is
byte-identical to an earlier prototype.

| Hypothesis | Native CMake option | Gradle property |
| --- | --- | --- |
| H1, lazy directories | `DEXKIT_EXPERIMENT_LAZY_DIRECTORIES=ON` | `-PexperimentLazyDirectories=ON` |
| H2, compact string uses | `DEXKIT_EXPERIMENT_COMPACT_STRINGS=ON` | `-PexperimentCompactStrings=ON` |
| H3, empty-parse memo | `DEXKIT_EXPERIMENT_NEGATIVE_STRINGS=ON` | `-PexperimentNegativeStrings=ON` |

Pass a CMake option as `build_native.py --define OPTION=ON`. The H3 payload
budget is `DEXKIT_EXPERIMENT_STRING_MEMO_BYTES` (default 1048576); zero forces
normal parsing. The budget is per query and covers live requested bit-array
bytes, not total process memory. H3 applies only to method batch queries.

Run `:dexkit:cmakeBuild :dexkit:jar :dexkit:test :dexkit-android:assembleRelease`
with the corresponding Gradle property and `-I tools/qaux-benchmark/force_tests.gradle`.
The init script forces an actual test execution after native flag changes;
UP-TO-DATE or FROM-CACHE is not new variant validation. The exhaustive native metadata check
also compares numbers, strings and opcodes over every demo method, including
empty and duplicate cases, cold concurrent access, and lazy/full transitions:

```sh
python3 tools/qaux-benchmark/build_native.py \
  --source-root /path/to/worktree --java-home /path/to/arm64-jdk17 \
  --output /path/to/artifacts/metadata-checks \
  --define DEXKIT_EXPERIMENT_LAZY_DIRECTORIES=ON \
  --define DEXKIT_EXPERIMENT_COMPACT_STRINGS=ON \
  --define DEXKIT_BENCHMARK_DIAGNOSTICS=ON \
  --define DEXKIT_BENCHMARK_METADATA_CHECKS=ON
/path/to/artifacts/metadata-checks/build/Core/dexkit_metadata_checks \
  /path/to/worktree/dexkit/apk/demo.apk
```

For memo counterexamples, build with `DEXKIT_EXPERIMENT_NEGATIVE_STRINGS=ON`
and `DEXKIT_BENCHMARK_MEMO_CHECKS=ON`, with diagnostics OFF. Run
`build/Core/dexkit_memo_checks` and `build/Core/dexkit_memo_budget_checks` from
that artifact. The latter alone injects a nothrow array allocation failure;
neither the native library nor the timing executable contains that injection.
The checks include concurrent quota contention, release/reuse, zero/oversized
budgets, positive/negative strings, and empty/tiny scopes. They are separate
from the QQ scores. The source, compiler and native hash remain recorded in
the artifact manifest.

## Raw metadata follow-up

`RAW-METADATA-PLAN.md`, `RAW-METADATA-REVIEW.md` and `RAW-METADATA-RESULTS.md`
record the second round, including rejected variants and immutable samples.
All options below default to OFF; all H options above remain independent.

| Behavior | CMake suffix after `DEXKIT_EXPERIMENT_` | Gradle property |
| --- | --- | --- |
| R1 raw cross-reference identities | `STRUCTURAL_DESCRIPTORS` | `experimentStructuralDescriptors` |
| R3 borrowed interface lists | `RAW_INTERFACES` | `experimentRawInterfaces` |
| Published cache hit / cold build separation | `DESCRIPTOR_FAST_HITS` | `experimentDescriptorFastHits` |
| Raw descriptor-input lookup, regressing | `RAW_DESCRIPTOR_LOOKUP` | `experimentRawDescriptorLookup` |
| R2 paged byte records, regressing output | `PAGED_DESCRIPTORS` | `experimentPagedDescriptors` |
| Isolated body alignment, ineffective fix | `ALIGNED_DESCRIPTORS` | `experimentAlignedDescriptors` |

Raw lookup requires R1; alignment requires R2; fast hits require R1 or R2.
The original R1 samples predate the lookup split: reproducing that bundle from
current source also requires raw lookup ON. Prefer exact recorded commits for
historical timings. The preferred R1+R3 comparison enables structural identities,
raw interfaces and fast hits, leaving the three regressing/ineffective options
OFF. The larger preferred combination additionally enables H1/H2/H3.

Generate the two bounded DEX fixtures outside the worktree:

```sh
python3 tools/qaux-benchmark/make_symbol_fixture.py --output /path/to/symbol-fixture
python3 tools/qaux-benchmark/make_symbol_fixture.py \
  --output /path/to/overload-fixture --same-name-overloads
```

Build a diagnostic artifact with `DEXKIT_BENCHMARK_DIAGNOSTICS=ON` and
`DEXKIT_BENCHMARK_SYMBOL_CHECKS=ON`, plus the candidate flags. Run
`build/Core/dexkit_symbol_checks /path/to/symbol-fixture/symbols.apk` and
`build/Core/dexkit_paged_descriptor_checks`. The symbol executable's `--dump`
mode takes an APK after that option. Freeze the complete stdout from a separate
all-flags-off artifact and byte-compare both fixture dumps for each candidate;
these include complete FlatBuffers, not just descriptor hashes. The committed
compressed oracles and manifests document the expected bytes. Cache checks
also exercise NULs, block/page boundaries, large records and retained views
during concurrent growth. `invalid-index` and `repeat-initialize` are separate
expected-SIGABRT invocations, not successful-use cases.

Timing artifacts must have diagnostics OFF and
`DEXKIT_BENCHMARK_DESCRIPTOR_WORKLOAD=ON`. For example:

```sh
python3 tools/qaux-benchmark/workload_sweep.py \
  --fixture /path/to/symbol-fixture/symbols.apk --output /path/to/new-sweep \
  --variant control /path/to/artifacts/control \
  --variant candidate /path/to/artifacts/candidate \
  --mode output --repeats 64 --pairs 6 --seed 2026091517
```

Modes/repetitions are `output`/64, `lookup`/256, `lookup-hot`/100000,
`lookup-prefix`/256 and `interfaces`/2000. Only `lookup-prefix` uses the overload
fixture. Positive/negative request timings include their result destruction;
complete lifecycle includes setup and bridge close. Lookup setup warms the
selected hit. The prefix miss currently has a different total byte length and
must not be described as an equal-length last-byte comparison.

`build_workload.py` can relink a changed harness against a frozen core archive
when its recorded native source trees match the checkout. It records separate
harness, executable and archive hashes and creates a new artifact directory.
For QQ, repeat full `run.py --mode verify --passes 11` after the final JAR build,
then use `sweep.py` as above with twelve main pairs and six separate confirmation
pairs, for both one and eleven passes. The same JAR, input fingerprints, options
and fixed memory probe must be used in verification and measurement. Never
run Gradle, native builds or another benchmark during a measured sweep.

## Ordinary candidate execution

`CANDIDATE-PIPELINE-EXECUTION.md`, `CANDIDATE-PIPELINE-REVIEW.md` and
`CANDIDATE-PIPELINE-RESULTS.md` describe the bounded ordinary Find refactor.
Batch and existing string admission remain unchanged. Both options default OFF:

| Behavior | CMake suffix after `DEXKIT_EXPERIMENT_` | Gradle property |
| --- | --- | --- |
| Candidate preparation/consumption and query lifetime | `CANDIDATE_PIPELINE` | `experimentCandidatePipeline` |
| Optional successful candidate re-slicing | `CANDIDATE_SLICES` | `experimentCandidateSlices` |

Pipeline requires INVERTED_STRINGS; slicing requires pipeline. When enabled,
successful candidates validate in the preparation worker unless re-slicing is
separately enabled and multiple original ranges can use several workers.
The evaluated re-slicing has no repeatable latency benefit and increases peak
footprint in multi-result cases. The results report preserves both improvements
and regressions; neither option is enabled by this experiment.

`make_candidate_fixture.py` builds cross-slice method/interface and reversed
ClassDef checks. `make_candidate_skew_fixture.py` builds bounded uneven DEX
inputs. With `DEXKIT_BENCHMARK_STRING_WORKLOAD=ON`, CMake builds the candidate
workload and runtime/coordinator/no-exceptions checks; diagnostic builds add the
private production-consumer integration checker. Use `workload_sweep.py` with
the archived plans. It checks the native executable's actual deployment target
against the engine manifest and requires common compiler settings before timing.

## Source attribution

QAuxiliary source files identify their terms as AGPL-3.0-or-later plus the
project EULA. The Java adapter retains an attribution notice; the extraction
also writes the source header beside generated local data. Copies of the AGPL
and QAuxiliary EULA are included under `licenses/`. Original sources:

- [DexKitTarget.kt](https://github.com/cinit/QAuxiliary/blob/01801ffd013c95781dd360704adf48dc42ee8aa6/app/src/main/java/io/github/qauxv/util/dexkit/DexKitTarget.kt)
- [DexKitDeobfs.kt](https://github.com/cinit/QAuxiliary/blob/01801ffd013c95781dd360704adf48dc42ee8aa6/app/src/main/java/io/github/qauxv/util/dexkit/impl/DexKitDeobfs.kt)
- [InjectDelayableHooks.java](https://github.com/cinit/QAuxiliary/blob/01801ffd013c95781dd360704adf48dc42ee8aa6/app/src/main/java/io/github/qauxv/core/InjectDelayableHooks.java)
- [QAuxiliary license](https://github.com/cinit/QAuxiliary/blob/01801ffd013c95781dd360704adf48dc42ee8aa6/LICENSE.md)

See `REPORT.md` for the inspected sample and observed compatibility results.

The sparse/dense descriptor prototype has completed its fixed comparison:
[results](HYBRID-DESCRIPTORS-RESULTS.md), [source review](HYBRID-DESCRIPTORS-REVIEW.md),
and [evidence manifest](evidence/hybrid-descriptors/v1/manifest.json). Both new
storage flags remain OFF by default; the report retains the residual dense-array
regressions and describes the next bounded hot-entry check.

The independent `vector<string>` experiment also completed its fixed 82-sweep
comparison: [results](VECTOR-DESCRIPTORS-RESULTS.md),
[source review and native lifetime checks](VECTOR-DESCRIPTORS-REVIEW.md), and
[evidence manifest](evidence/vector-descriptors/v1/manifest.json). It retains
sparse QQ memory savings and improves selected lookups, while cold/full output
and physical-peak regressions prevent a general replacement recommendation.
`DEXKIT_EXPERIMENT_VECTOR_DESCRIPTORS` (Gradle `experimentVectorDescriptors`) and
its `VECTOR_DESCRIPTORS_NO_PROMOTION` control (Gradle
`experimentVectorDescriptorsNoPromotion`) default OFF. The control requires the
vector option. Direct native Bean consumers must follow the documented borrowing
contract; ordinary Java/Kotlin APIs manage it internally.
