# Ordinary find extraction

Base: `699c01dc28845a32cc9c1051fe629ba36556a8fe`.

The user requested independent measurements of Empty task bypass and moving
result payloads on the existing Small path, and removal of the candidate
pipeline's exception recovery machinery. String admission and Batch behavior
remain fixed. The pipeline and optional slicing remain default OFF.

## Work plan

- [ ] Extract Empty task bypass and moving result concatenation behind separate
  measurement switches, without requiring the candidate pipeline.
- [ ] Remove QueryRun, exception tracking, catch/rethrow paths and the extra
  condition-variable drain. Use existing futures for normal completion, and
  release owning candidate captures before publishing each task result.
- [ ] Verify ordered results, actual Empty submission counts, retained-wrapper
  cleanup and normal completion on the externally owned shared scheduler.
  Run native, JVM/JAR and Android ABI checks, including no-exceptions builds.
- [ ] Freeze comparable control, Empty, move and combined artifacts. Measure
  QQ and representative negative/positive method/class cases with matched
  compiler settings, fresh processes, memory and repeated comparisons.
- [ ] Record separate benefits and costs, archive evidence, and choose defaults
  from the measurements. Commit and push the authorized fork branch.

## Measurement contract

The control uses the same B/Small flags as the previous comparison, with both
candidate pipeline switches OFF. The two extracted switches vary independently.
The existing pipeline's combined results cannot establish the benefits of
either extraction. Old evidence is immutable and kept separate from this run.

Only a proven `QueryPlan::Route::Empty` bypasses submission. A producer rejection
continues to use original ranges. Moving results preserves concatenation order,
vector growth policy and final descriptor representative selection.

Candidate work completes normally through the existing futures. Owning task
captures are destroyed before their futures become ready, even if a backend or
shared future retains the completed callable. The executor detaches while the
query context remains alive. This work adds no exception or OOM recovery policy.
