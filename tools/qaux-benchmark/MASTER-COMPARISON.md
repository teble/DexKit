# Current combination versus master

This is a direct paired comparison with local `master` at
`1d936bd9efa38e00b7336ef112e73ba4048324e6`, not a sum or multiplication of
earlier best5-relative percentages. The evaluated combination includes all
previously retained best5 switches and follow-up candidates 1, 3 and 5.
Field reverse indexes are built on demand; this is part of candidate 1,
not an additional independent optimization.

In independent confirmation, the unchanged QQ workload uses about 477--481
MiB less peak memory (23.8%) and completes 37.2%/35.1% sooner for one/eleven
passes. When complete reverse-field construction is added before close,
savings remain about 352--354 MiB (17.6%) and lifecycle time falls
28.8%/28.9%. Both batches agree on the direction of all four comparisons.

## Configuration and source identity

The combination enables LAZY_DIRECTORIES, COMPACT_STRINGS, NEGATIVE_STRINGS,
STRUCTURAL_DESCRIPTORS, DESCRIPTOR_FAST_HITS, RAW_INTERFACES,
FIELD_IDENTITY_SPLIT, COMPACT_INVOKES and SINGLE_RELATION. Each name has the
`DEXKIT_EXPERIMENT_` prefix. The descriptor-pointer, raw-source-file, paged,
aligned and raw-descriptor-lookup experiments remain OFF. Experimental
defaults are unchanged.

The exact master native SHA256 is
`03eddd050d473950dbfc2d1757f1f541f11e168036b4431024c8c343ad5ce004`;
the combination is
`2347a204a16ecf3b0855189ee2c3e62b27e8c1cdff476146ecb7964e6d7422ba`.
The saved master source trees match the current clean master checkout.
Both snapshots use the same Apple clang 21 compiler, arm64 Release options
`-O3 -DNDEBUG`, SDK, deployment target, and disabled internal metrics. No
diagnostic code is active in either timed snapshot. The JVM library source
is unchanged from master, and both use the same JAR, SHA256
`aad51ff2f604056be1a1b2cbb0a41adae32ac17e855e7eac8a6db10898383e9a`.
The benchmark harness is recorded at `a4648d9`; native source identities are
the separate artifact-manifest identities above.

## Workloads and validation

Inputs are the pinned QQ 9.3.55 APK (41 DEX files), QAuxiliary corpus and JDK
used in the earlier experiments, with four query workers. Both configurations
pass eleven complete frozen-result verification passes for both scenarios.

* **normal** executes the unchanged QQ queries. These queries do not consume
  field reverse indexes, so the candidate can leave them unbuilt.
* **tail** runs the same queries and then resolves
  `Lcom/tencent/mobileqq/data/MessageRecord;->msg:Ljava/lang/String;`, fetching
  its readers and writers before close. This builds the global reverse indexes
  across all 41 DEX files. Both variants match the independent ordered oracle:
  399 readers and 313 writers. Prior diagnostic verification establishes equal
  completed reverse-index capacities. This scenario includes the deferred work
  but does not model repeated reverse-query throughput.

Complete lifecycle includes create, all queries, the final operation where
applicable, and close. Library loading is recorded separately. Peak memory
is the host process physical footprint, including the shared JVM overhead.
This is a host query benchmark, not an Android device timing result.

## Results

Each cell uses six balanced randomized fresh-process pairs. The independent
main and confirmation batches use seeds 2026091537 and 2026091538. All 96
valid process samples are retained, and no builds, heavy tests or archiving
run alongside measurements. Negative percentages favor the combination.
Intervals are exploratory paired-median 95% bootstrap intervals.

Confirmation:

| Scenario | QQ passes | Complete lifecycle change | Peak footprint change | Paired peak saving |
| --- | ---: | ---: | ---: | ---: |
| Original QQ; reverse indexes remain unused | 1 | -37.18% [-41.49, -33.68] | -23.80% [-24.01, -23.49] | 476.64 MiB |
| Original QQ; reverse indexes remain unused | 11 | -35.12% [-36.97, -33.94] | -23.82% [-24.15, -23.36] | 480.50 MiB |
| QQ followed by complete reverse index construction | 1 | -28.78% [-32.16, -26.84] | -17.59% [-17.95, -17.36] | 352.44 MiB |
| QQ followed by complete reverse index construction | 11 | -28.87% [-31.08, -27.30] | -17.56% [-18.10, -17.50] | 354.27 MiB |

First batch:

| Scenario | QQ passes | Complete lifecycle change | Peak footprint change | Paired peak saving |
| --- | ---: | ---: | ---: | ---: |
| Original QQ; reverse indexes remain unused | 1 | -40.62% [-46.46, -39.32] | -23.82% [-23.99, -23.73] | 477.20 MiB |
| Original QQ; reverse indexes remain unused | 11 | -34.73% [-35.31, -34.48] | -23.78% [-24.11, -23.70] | 480.75 MiB |
| QQ followed by complete reverse index construction | 1 | -31.02% [-36.13, -22.50] | -17.58% [-17.86, -17.39] | 353.37 MiB |
| QQ followed by complete reverse index construction | 11 | -30.99% [-35.93, -29.10] | -17.81% [-18.08, -17.64] | 359.93 MiB |

Confirmation absolute medians follow. Paired percentages above are medians
of within-pair changes, not ratios of these separately computed medians.

| Scenario | QQ passes | Lifecycle medians, master to combination | Peak footprint medians, master to combination |
| --- | ---: | ---: | ---: |
| normal | 1 | 2.730 to 1.684 s | 2001.82 to 1525.05 MiB |
| normal | 11 | 8.843 to 5.737 s | 2016.99 to 1536.31 MiB |
| tail | 1 | 2.538 to 1.769 s | 2003.97 to 1651.29 MiB |
| tail | 11 | 8.402 to 6.004 s | 2016.69 to 1662.35 MiB |

These cumulative gains are specific to the evaluated QQ workloads. Earlier
consumer counterexamples remain in FOLLOWUP-RESULTS.md; their best5-relative
regressions must not be relabeled as master-relative results. The isolated
field-deferral benefit after eventual construction remains the separate
field-tail comparison. The variant with field deferral disabled while
retaining compact invokes and single-requirement matching has not been
directly measured.

## Evidence and reproduction

`evidence/followup/master-comparison/` contains all eight summaries and sample
sets, the exact local driver, source/compiler identity proof, both native
manifests, and the frozen final-field oracle. `process-records.tar.gz` includes
524 original JSON/log records from the four complete verification runs
and all timed runs. Every member and the archive itself were read back and
verified against SHA256 and size metadata in `archive.json`. APKs, JARs and
generated native/class binaries are excluded from the archive.

Use the existing `run.py`/`sweep.py` commands in FOLLOWUP-REPRODUCE.md, replacing
the best5 control with the exact master snapshot and its matching verification
directory. Use `--final-field-rw` and `--field-rw-expected` only for the tail
scenario. Each archived launcher and metadata record includes the full command
and input hashes. No production code or binary was changed for this comparison.
