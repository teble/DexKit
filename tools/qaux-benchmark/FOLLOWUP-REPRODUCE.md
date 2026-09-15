# Reproducing the follow-up experiments

Use the frozen corpus, APK hash, Java runner and validation protocol in
[README.md](README.md). Results and decisions are in
[FOLLOWUP-RESULTS.md](FOLLOWUP-RESULTS.md). Performance numbers are host
measurements; Android packaging validation is a separate check.

## Configurations

Every experiment starts from best5, with these six CMake flags ON:

```text
DEXKIT_EXPERIMENT_LAZY_DIRECTORIES
DEXKIT_EXPERIMENT_COMPACT_STRINGS
DEXKIT_EXPERIMENT_NEGATIVE_STRINGS
DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS
DEXKIT_EXPERIMENT_DESCRIPTOR_FAST_HITS
DEXKIT_EXPERIMENT_RAW_INTERFACES
```

All other experiment flags are OFF unless listed below. Keep PAGED_DESCRIPTORS,
ALIGNED_DESCRIPTORS and RAW_DESCRIPTOR_LOOKUP OFF in every listed configuration.

| Configuration | Additional CMake experiment flags ON | Gradle properties ON |
| --- | --- | --- |
| Field split | FIELD_IDENTITY_SPLIT | experimentFieldIdentitySplit |
| Pointer descriptors | POINTER_DESCRIPTORS | experimentPointerDescriptors |
| Contiguous invokes | COMPACT_INVOKES | experimentCompactInvokes |
| Raw sources | RAW_SOURCE_FILES | experimentRawSourceFiles |
| Preferred addition | SINGLE_RELATION | experimentSingleRelation |
| QQ-conditional combination | FIELD_IDENTITY_SPLIT, COMPACT_INVOKES, SINGLE_RELATION | All three corresponding properties |

CMake names in the second column have the `DEXKIT_EXPERIMENT_` prefix.
Each artifact manifest records the exact source commit, source trees, compiler,
native SHA256 and full configure command. These are the authority for historical
reproduction; do not silently rebuild a historical label from a newer checkout.

## Native artifacts and fixtures

Build fresh immutable directories with `build_native.py`. For example, from
the experiment checkout with a JDK 17 path and an external output directory:

```bash
python3 tools/qaux-benchmark/build_native.py \
  --source-root "$DEXKIT_ROOT" --java-home "$JDK_ROOT" \
  --output "$BENCH_OUT/single" --jobs 2 \
  --define DEXKIT_EXPERIMENT_LAZY_DIRECTORIES=ON \
  --define DEXKIT_EXPERIMENT_COMPACT_STRINGS=ON \
  --define DEXKIT_EXPERIMENT_NEGATIVE_STRINGS=ON \
  --define DEXKIT_EXPERIMENT_STRUCTURAL_DESCRIPTORS=ON \
  --define DEXKIT_EXPERIMENT_DESCRIPTOR_FAST_HITS=ON \
  --define DEXKIT_EXPERIMENT_RAW_INTERFACES=ON \
  --define DEXKIT_EXPERIMENT_SINGLE_RELATION=ON \
  --define DEXKIT_BENCHMARK_RELATION_WORKLOAD=ON \
  --define DEXKIT_BENCHMARK_DESCRIPTOR_WORKLOAD=ON \
  --define DEXKIT_BENCHMARK_SOURCE_WORKLOAD=ON
```

Build the independent best5 control with SINGLE_RELATION OFF. For diagnostics,
use a separate artifact with BENCHMARK_DIAGNOSTICS and the required
BENCHMARK_RELATION_CHECKS, BENCHMARK_SYMBOL_CHECKS and BENCHMARK_SOURCE_CHECKS
flags ON. Timed libraries must have diagnostics and both internal metrics
options OFF. Do not time sanitizer or diagnostic builds.

Sanitizer manifests record `-O1 -g -DNDEBUG -fsanitize=address,undefined
-fno-omit-frame-pointer` for C/C++, sanitizer linker flags, and the executions
use `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`.

Generate the invocation fixtures with `make_invocation_fixture.py`:

| Fixture | --methods | --fanout | --sparse-every |
| --- | ---: | ---: | ---: |
| tiny | 8 | 2 | 4 |
| mixed | 64 | 1024 | 4 |
| giant | 1 | 131072 | 0 |

Use `make_source_fixture.py --classes-per-dex 0` and `30000` for the small
and wide source fixtures. The field fixture generator and all fixture hashes
are retained with their evidence. The dense descriptor fixture has 60,000
methods and 60,000 fields; `make_dense_descriptor_fixture.py` generates it.
Each generator requires its own empty `--output` directory.

## Correctness and timing

Run the corresponding `dexkit_*_checks --dump` executables in each artifact's
`build/Core/` directory. Compare complete buffers against the separately built
best5 oracle, not only against an in-process full-cache reference. The final
invocation checker has 29 cases and verifies the older 24 case buffers remain
unchanged. Relation checks cover all reference and definition IDs and six
initialization orders. Preserve the existing frozen QQ expected results.

Run `run.py --mode verify --passes 11` for each normal native library before
using `sweep.py`. Supply its verification directory to the corresponding
`--variant LABEL LIBRARY VERIFICATION` argument. Both tools' complete commands,
input hashes, JAR/probe hashes and reports are in the process-record archives.

Native counterexamples use:

```bash
python3 tools/qaux-benchmark/workload_sweep.py \
  --fixture "$FIXTURE_APK" --mode invoke-early --repeats 1024 \
  --pairs 6 --seed 2026091532 --output "$BENCH_OUT/measurement" \
  --variant best5 "$BENCH_OUT/best5" \
  --variant single "$BENCH_OUT/single"
```

Each summary records the exact mode, repetitions, seed, binary hashes and
balanced order. Use a fresh output directory for each run, retain every valid
sample, and run without concurrent builds or heavy tests. Complete lifecycle
and first/repeated API costs are separate metrics. Early, late/miss and
multiple modes have distinct predicates and must only be compared within
their own A/B pair. In field workloads, `negative_ns` is the reverse getter
leg; in source-hot it is unused. Do not treat either as a universal negative
query metric.

## Components and evidence

### Completing deferred field work after QQ

For the cumulative comparison against the original master snapshot, see
[MASTER-COMPARISON.md](MASTER-COMPARISON.md) and its source identity proof.

The later `28e0c32` harness adds an opt-in final operation to both `run.py`
and `sweep.py`:

```bash
--final-field-rw 'Lcom/tencent/mobileqq/data/MessageRecord;->msg:Ljava/lang/String;'
```

It runs once after all requested QQ passes, before close. Use the same pinned
APK, corpus, JDK, JAR and memory probe. First run the independent best5 binary
in verify mode with this option and `--passes 11`, then freeze the new result:

```bash
python3 - "$BENCH_OUT/tail-oracle/report.json" "$BENCH_OUT/field-rw-expected.json" <<'PY'
import json
import pathlib
import sys
tail = json.loads(pathlib.Path(sys.argv[1]).read_text())['final_field_rw']
keys = ['descriptor', 'readers_count', 'writers_count', 'readers', 'writers']
pathlib.Path(sys.argv[2]).write_text(json.dumps({k: tail[k] for k in keys}, indent=2) + '\n')
PY
```

Add `--field-rw-expected "$BENCH_OUT/field-rw-expected.json"` to every
subsequent verification and paired measurement. Reverify all normal variants
for eleven passes with the new harness and both options; older verification
directories do not certify this added work. Measurement checks the ordered
oracle's counts and requires the matching successful verification. The frozen
original QQ results remain unchanged. Two batches use six pairs per cell,
seeds 2026091535/2026091536, one/eleven passes, and independent comparisons
of field-only and the three-switch combination against best5.

`evidence/followup/field-tail/` contains the exact local driver, native
manifests, frozen field oracle, all eight summaries and a separate archive
with 539 original JSON/log files. These records supplement the original
five-candidate archives. Diagnostic cache census confirms complete reverse
indexes before close; diagnostic binaries are never timed. No library source
or binary changes are part of this harness follow-up.

### Library component checks

For each configuration, run native/JAR, a forced fresh JVM suite and release
Android packaging with the corresponding explicit properties:

```bash
bash gradlew -I tools/qaux-benchmark/force_tests.gradle \
  :dexkit:cmakeBuild :dexkit:jar :dexkit:test :dexkit-android:assembleRelease \
  --max-workers=2 --console=plain
```

Add all configuration properties as recorded in `components-*.json`; omitting
them uses the default-OFF engine. The component logs record 71 executed tests
per evaluated configuration and the release AAR contains four native ABIs.
Use the JDK/Android SDK setup from the project guide.

`evidence/followup/process-records/archive.json` lists every archive and member
hash and size. The six `.tar.gz` files contain all 167 timing stages, including
the individual JSON reports and process logs. Other evidence directories keep
diagnostic output, fixture and build manifests, component logs and independent
binary result oracles. Extract with standard tar/gzip tools; the input APKs
and compiled libraries are intentionally outside the repository.
