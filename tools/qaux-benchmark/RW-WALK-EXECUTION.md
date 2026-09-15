# Skip the deferred RW instruction walk

Status: active; source implementation is ready for same-source validation.

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
