# Contiguous field-row source review

The authorized [Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)
completed this review in 9m 16s. Its full final response was read. The fixed
target was 4247372bede8bf9460076825ee44c838e94df16b relative to 2ad0f2c.
It examined CompactFieldIndex, DexItem declarations/build/getter paths,
Hungarian and field matching, admission/normalization/cache initialization,
diagnostics, CMake and both Gradle files, the field/relation checkers, independent
validator, workload and sweep. It did not run tests or review formal timings.

No new production-path correctness blocker was identified. BeginMethod returns
the vector object's address, so payload growth does not invalidate the append
handle. Each offset precedes append, each row length is checked before narrowing,
and reads validate by subtraction before subspan. The existing cache protocol
fills all forward rows before publication and later requests claim only missing
flags, so RW/caller/full initialization does not append to published field rows.
The container depends on that one-build protocol: resize is not a reset/rebuild
API and the object does not support independent concurrent writes.

Borrowing the const span preserves target positions, duplicate requirements and
the existing map/p/vis algorithm. Its current pair is copied before invoking the
value-taking judge. GetUsingFields still creates independent Beans and serialized
results in row order; it does not return a borrowed view to callers.

Local review adopted the diagnostic limits. prepared_items counts logical solver
targets under COMPACT_FIELDS, not copied targets; requirements/matrix/p/vis work
remains. The plan now says live buffers and capacity growth rather than allocation
counts. Moved bytes represent element storage inferred from full-vector growth,
not measured memory traffic. Old+new capacity overlap describes one DEX payload
growth and excludes concurrent DEXes, columns and other allocations.

The independent getter oracle covers every method in its unique-definition
fixtures. FindMethod enumeration alone does not cover all raw IDs in arbitrary
DEXes because of descriptor deduplication; the relation checker separately walks
every raw MethodId and covers references and duplicate definitions. Held-span
checks include an actual queued RW admission only when FIELD_IDENTITY_SPLIT is
ON, as in this nine-switch candidate and its diagnostic/sanitized checks.

Getter setup includes method enumeration and its output destruction. Each result
is destroyed within the API interval, and method IDs/query builders are destroyed
inside the outer lifecycle. The first getter interval includes forward warmup and
identity resolution; it is not a serialization-only benchmark. The candidate's
timing includes both contiguous storage and removal of solver target-row copies.
The next RW-only experiment remains independent with COMPACT_FIELDS OFF.
