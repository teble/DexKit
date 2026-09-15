# Deferred RW instruction-walk review

The authorized [Pro conversation](https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3)
completed its fixed-source review of b57b687afa84bf3315b84204a2d49e3c84ec1dc1,
whose parent is 2616a668e55f8ba4e290f7036bf1a89a6c87728b. It used the selected
GitHub connector and reported no new production-path correctness blocker.

The reviewed material includes the actual diff, InitCache/Begin/Finish/Wait,
admission and InitDexCache dependency/publication flow, normalization,
cross-DEX aggregation, instruction-width helpers, relation checks/workload,
execution plan, Core CMake and both Gradle changes. This is a source review;
Pro did not rerun the checks or evaluate the later formal timing samples.

## Findings and local resolution

* Eligibility must use the claimed missing flags. A first RW request still
  carries missing forward-field extraction and therefore scans. Ready forward
  inputs permit skipping the otherwise unproductive instruction loop; reverse
  fills, annotations, identities, aggregation and publication remain outside
  that decision. A later full request can also qualify when every instruction
  output is already ready; not every full request must scan.
* Normalization does not add Caller-to-Invoking dependencies. Callers must
  request them or have prepared them already; the seventh test schedule does
  the latter. The option does not make arbitrary incomplete internal flags
  valid. Inspection confirms the old unproductive loop would not repair such
  flags either.
* Standalone compilation/results establish compatibility with eager field
  coupling. Actual deferred coverage comes from FIELD_IDENTITY_SPLIT builds,
  whose trace has eligible calls and whose complete outputs agree. No broader
  standalone branch-coverage claim is made.
* The instruction log occurs after that loop and before reverse/annotation
  fills, not at InitCache completion or publication. methods counts methods
  with code; instructions counts width-walk iterations, including payload
  entries. walk=false still leaves method/edge traversal in reverse filling.
  A positive control count is only required for these nonempty-code workloads;
  a code-less DEX can legitimately report zero. The report and execution plan
  use these narrower meanings. Diagnostics never enter formal timing.
* Results are destroyed inside the native API intervals. Full-first work is
  included in setup, and remaining query/metadata destruction in lifecycle.
  negative_ns is the second, reverse API leg rather than a no-hit query.
  Warm API and process-peak observations cannot be assigned directly to a
  one-time instruction-scan deletion.

The suggested bounded test improvement adds assertions around the seventh
schedule: before joint reverse admission, both forward inputs are ready and
both reverse caches/aggregates are pending; afterward, both are ready before
any getter can fill a gap. After formal timing, the checker was recompiled and
linked to unchanged immutable Core archives. Both fixtures pass all assertions
in diagnostic control/candidate and the sanitized candidate, retaining the
original complete ordered output bytes in all six reruns. No measured library
or workload executable changed. No additional production-source review is
claimed for this bounded checker-only follow-up.
