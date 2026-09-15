# Contiguous forward-field rows

Status: implementation and validation in progress.

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
   forward/late-reverse/full-first relation lifecycle. Include construction,
   preparation, output destruction, close and process peak. Retain regressions.
5. Separately record logical row columns, payload capacities, allocation counts
   and vector-growth overlap. These counts are not estimates of process peak.
   Review the concrete pushed source and evidence in the authorized Pro task.

The subsequent RW-only instruction-walk experiment stays independent with
COMPACT_FIELDS OFF. Neither candidate becomes part of the nine-switch control.
