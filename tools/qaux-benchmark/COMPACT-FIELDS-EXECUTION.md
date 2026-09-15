# Contiguous forward-field rows

Status: complete. See COMPACT-FIELDS-RESULTS.md and COMPACT-FIELDS-REVIEW.md.

1. Compare the nine-switch control with only COMPACT_FIELDS added. All newer
   string, batch and single using-field options remain OFF. The new Core and
   Gradle option defaults OFF.
2. Append each field ID/access-direction pair directly to a per-DEX vector.
   Keep size_t offsets and checked uint32_t row lengths; method traversal need
   not follow method IDs. Preserve order, duplicates, empty and code-less rows.
   Publish through the existing admission/cache barriers. Borrow the resulting
   const spans in the existing field solver, without changing its matching or
   positional multiplicity algorithm. Reverse storage remains unchanged.
3. Validate all 48 field query cases and every public GetUsingFields row against
   independently decoded fixture rows, including unresolved identities. Compare
   complete ordered bytes through cold/full/repeat/concurrent sequences. Hold
   raw published spans across queued reverse warmup, calls and InitFullCache.
   Include standalone, ASan/UBSan, frozen QQ, JVM and four Android ABIs.
4. Measure finite six-pair batches and independent confirmation for QQ 1/11
   rounds; long/short/sparse/multiple matching; long getter output; and the
   forward/late-reverse/full-first relation lifecycle, plus QQ with the real
   final RW operation after eleven passes. Include construction,
   preparation, output destruction, close and process peak. Retain regressions.
5. Separately record logical row columns, payload capacities, live buffer counts
   and vector capacity growth. The old+new overlap covers one DEX payload growth;
   it excludes concurrent DEXes and other storage. These are not malloc counts,
   measured memory traffic or estimates of process peak.
   Review the concrete pushed source and evidence in the authorized Pro task.

The subsequent RW-only instruction-walk experiment stays independent with
COMPACT_FIELDS OFF. Neither candidate becomes part of the nine-switch control.

## Validation

The same-source control and candidate pass all 48 method/class queries and
complete ordered getter output for small, short, long and unresolved-field
fixtures. The long fixture has 3,627 defined methods and 921,631 ordered field
uses. Standalone and ASan/UBSan candidates pass the small, long and unresolved
oracles. Both relation fixtures pass all six schedules in the diagnostic
control/candidate and sanitized candidate, including held published spans.
The deferred RW assertions use FIELD_IDENTITY_SPLIT=ON; standalone field-query
checks do not independently establish deferred RW behavior.

Both normal libraries pass eleven frozen QQ rounds, with and without the
final field operation. Its 399 readers and 313 writers agree with the existing
independent ordered control. The rerun completes 71 JVM tests and four Android
ABIs. An earlier disk-full event interrupted Android, long-check output and
QQ-tail record writing. Old reproducible Android build directories were removed
after saving their caches; affected checks then passed. The failed logs remain
separate from the successful records and no formal timing ran during recovery.

The authorized fixed-source review found no new production-path correctness
blocker; COMPACT-FIELDS-REVIEW.md records its scope and diagnostic wording
corrections. Formal builds disable all trace/metrics options. No performance
conclusion is inferred from source review or logical storage counts.

Both finite batches complete: 22 sweeps / 264 process samples. Ordinary QQ
lifecycle and peak improvements repeat, as do material long-row peak increases.
The result report preserves warm-query regressions, unresolved getter/multiple
and delayed-RW effects, absolute peaks and all paired intervals. The next
mechanism remains independent against the nine-switch control.
