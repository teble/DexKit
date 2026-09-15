# Skip the deferred RW instruction walk

Status: complete. See RW-WALK-RESULTS.md and RW-WALK-REVIEW.md.

1. Compare the nine-switch control with only SKIP_RW_WALK added. COMPACT_FIELDS,
   SINGLE_USING_FIELD and all newer string/batch options remain OFF. The Core
   and desktop/Android Gradle options default OFF.
2. In InitCache, skip the method instruction loop only when field reverse rows
   are needed and no opcode/string/field-use/invocation/number extraction is
   missing. A co-requested caller reverse table can consume its ready inputs.
   Preserve all resizes, reverse fills, annotations, normalization, identity
   resolution, aggregation, admission and cache publication. Caller-only work
   retains its existing path.
3. Run complete ordered field-query/getter oracles, including unresolved field
   identities; standalone and ASan/UBSan builds; both relation fixtures and seven
   schedules, including both reverse tables requested after forward warmup.
   Verify eleven frozen QQ rounds with/without the real final field RW operation,
   all 71 JVM tests and four Android ABIs.
4. Separately count actual method/instruction visits for RW-bearing InitCache
   calls. Eligible control calls must scan and eligible candidate calls must
   visit zero methods/instructions. Mixed first/full initialization must still
   scan. Compare final table censuses and ordered outputs. Diagnostics are OFF
   in every timing artifact; no extra expected instruction scan is introduced.
5. Measure two independently ordered batches, each with six balanced pairs:
   original QQ 1/11 rounds, QQ plus real final RW 1/11 rounds, and native field
   forward/late-reverse/full-first lifecycle. Include construction, preparation,
   output destruction, close and process peak. Retain guards and regressions.
   Review the concrete pushed source in the authorized Pro conversation.

This is the last mechanism in the ordered next-round plan. It remains an
independent experiment; prior candidate percentages are not added together.

## Completed validation

Fixed source b57b687 builds the same-source normal control/candidate, diagnostic
control/candidate, standalone candidate and ASan/UBSan candidate. The normal
libraries pass the four small/short/long/unresolved 48-query and complete getter
oracles. Standalone and sanitizer builds also pass small/long/unresolved checks.
Both relation fixtures pass seven schedules in diagnostic control/candidate and
the sanitized candidate, with complete ordered bytes equal across artifacts.
The standalone checks establish compatibility with the original eager coupling;
they do not independently establish deferred RW coverage.

Both normal libraries pass eleven frozen QQ rounds, with and without the final
field operation, retaining all 399 ordered readers and 313 writers. The required
cmakeBuild, jar, test and Android assembleRelease tasks pass with 71 JVM tests
and arm64-v8a, armeabi-v7a, x86 and x86_64 libraries. All five platform caches
enable only the nine controls plus SKIP_RW_WALK, with diagnostics disabled.

Separate diagnostics show 41 eligible QQ calls: control visits 1,921,832 code
methods and takes 40,384,589 instruction-loop steps; candidate visits zero.
The native delayed-RW fixture changes from 134 methods / 50,576 steps to zero;
full-first initialization retains those same visits in both variants. Every
final forward-field, reader and writer census agrees. The seventh schedule's
RW+caller flag value 0x1400 appears for all three fixture DEXes and takes zero
visits in both diagnostic and sanitized candidates, versus positive control
visits. Complete call and field outputs still agree.

The rw_only diagnostic means no instruction output remains to be extracted,
not that the request contains only one flag. Counts describe existing loop
iterations, including any payload entries visited as one item; they are not
16-bit code-unit counts or inferred timings. Logs are emitted after the walk,
before reverse-table fills. Formal builds contain none of these counters.

All fourteen sweeps / 168 process samples and independent confirmation are
complete. Final QQ RW API gains repeat, as does one-pass lifecycle improvement;
eleven-pass lifecycle and memory remain unresolved. The report retains the
native forward guard increase and the following-writer confirmation increase.
The fixed-source review found no production-path blocker. Its suggested
readiness assertions pass on both relation fixtures in all three applicable
builds, relinked against the same Core archives after timing.
