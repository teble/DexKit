# Raw metadata experiments

## Objective and scope

The user authorized a new round of Pro-guided architecture experiments after
the first three hypotheses were completed. Seek lower complete-workflow time
and/or lower peak memory, preferring memory savings without time regression.
Existing representations are candidates for replacement, not fixed design
requirements. The earlier phase's three-hypothesis limit and 10%/15% expansion
guideline do not define the acceptance threshold of this round.

Work stays on `codex/raw-metadata-experiments`, based on `42e6c7f`. The original
checkout is separate. Review pushes, when needed, go only to `teble/DexKit`.
The three earlier experiments remain OFF during isolated comparisons.

## Plan and current state

1. **Complete:** obtain Pro's complete advice against the fixed source and
   prepare a fresh baseline/calibration using the existing verified corpus.
2. **Complete:** select concrete independent candidates from the advice, check their source
   assumptions, and add default-OFF prototype switches. Record each candidate's
   representation, avoided work, expected costs, and counterexamples before
   looking at its performance samples.
3. **In progress:** compare exact results and targeted lifetime/concurrency cases, then measure
   complete short and reused lifecycles. Do not time builds or diagnostics
   concurrently with formal samples.
4. Independently confirm useful candidates; investigate actual regressions.
   Review the resulting source and evidence with Pro when it can resolve a
   concrete design or correctness question. Report supported tradeoffs and
   remaining uncertainty, without requiring a successful optimization.

## Measurement contract

Reuse QQ 9.3.55 and QAuxiliary `01801ffd`, the frozen full result/flow contract,
arm64 JBR 17.0.6, fixed 256 MiB JVM heap, four engine threads, and the existing
native builder and memory probe. A new native baseline is built from the phase
base with all experiments OFF. Historical scores are context, not the control
for new measurements. Preserve every valid sample and immutable artifact.

Calibrate with six paired A/A runs for one and eleven passes. For admitted
candidates use twelve balanced AB/BA pairs in both modes, followed by six new
confirmation pairs for useful results. Targeted counterexamples are separate
from QQ scores. Report construction, first pass, repeated API sum, close,
complete lifecycle, and physical footprint; native logical storage is only
attribution. Preserve workload-specific peaks and memory after close.

Time preference is not a claim that zero regression can be statistically
proved. Report paired changes and uncertainty. Distinguish clear regressions,
supported improvements, and intervals that cannot resolve a tradeoff. Do not
interpret failure to reject a difference as proof of non-inferiority, select a
regression margin after seeing samples, or keep sampling until a result wins.
If a user-facing time budget is needed for adoption, report the measured upper
bound/tradeoff rather than inventing an accepted budget.

Fresh baseline `d0d5df4` (native source identical to `42e6c7f`) passed eleven
complete verification passes. Native SHA-256:
`4f4a1e6a86d9d40c8f51eef96a9d9b545f4ba2b44b8ca0898bc5a80fb4b1e110`.
Six A/A pairs per lifetime, seed `2026091504`, gave these paired changes:

| Lifetime | Complete time, median [95% interval] | Peak footprint, median [95% interval] |
| --- | --- | --- |
| One pass | +0.76% [-2.50%, +7.04%] | +0.17% [-0.19%, +0.59%] |
| Eleven passes | -0.49% [-2.53%, +1.40%] | -0.20% [-0.33%, -0.04%] |

Identical binaries can show a small coherent drift, including a footprint
interval excluding zero. Small candidate differences require independent
confirmation and should not be elevated into universal guarantees. Raw files
are retained in the external `raw-metadata/formal/aa-p1` and `aa-p11` directories.

## Facts to preserve

- Raw string bytes and the fixed DEX ID tables are already shared views. A
  second compressed string copy does not remove the retained raw image.
- Descriptor strings currently support cross-DEX resolution, descriptor
  lookup, result deduplication, serialization, and stable `string_view` storage.
- Different DEX IDs do not imply different textual symbols. Descriptor-based
  deduplication and current output order must remain equivalent.
- Relationship order, duplicates, empty rows, nested matching witnesses,
  publication/close coordination, and returned value lifetimes must be kept.
- All conversion, allocation, growth, synchronization, rebuilding, and cleanup
  costs belong in the complete comparison. Prior arena experience showed that
  faster destruction alone can still make the whole workflow slower.

## Pro consultation

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

The new request was sent once against fixed source `42e6c7f`. The full answer
was read and copied after completion. Pro reported reading the requested
descriptor, cross-reference, lookup, aggregation, Bean, matcher and Reader
paths, and checking the default-OFF CMake switches. It did not run tests or
recalculate the corpus. The consequential source claims were checked locally.

Pro recommends raw structural comparisons, a paged stable descriptor cache,
and a raw interface view, in that order. It rejects immediate conversion to
result-owned strings because repeated large outputs would rebuild signatures.
The current cross-reference iteration and signature-based result deduplication
must remain; changing unresolved-reference handling would be a separate fix.

## Admitted prototypes

### R1: structural member comparison and descriptor lookup

Replace only the equality predicates in the existing cross-reference loops.
Compare raw name/type contents and ordered parameter types without allocating
a member identity table. Input lookups parse once into borrowed string slices,
keep the same class member search domain, and compare against raw metadata.
Return values and descriptor-based deduplication keep their current behavior.

Keep the dense output cache for this isolated test. More descriptors can now
be first materialized by concurrent output requests, so add one atomic ready
byte per member and striped cold-fill locks. The hot path acquires a ready
byte before reading the immutable string. These publication costs are included
in R1, not assumed to be free or deferred to R2.

Expected benefit: fewer internal string constructions and retained buffers,
especially during initial cross-reference construction and failed lookups.
Counterexamples: many same-name/long-prototype candidates, hot narrow lookups,
first output after warm-up, and concurrent first output of the same member.

### R2: paged, stable descriptor byte storage

Replace the dense optional strings with a small atomic page directory. Pages
contain 256 atomic record pointers. Records contain a size_t byte length and
the final NUL-terminated descriptor in non-moving byte blocks. Use eight cold
fill stripes per cache, allocate blocks on demand starting at 4 KiB and grow
up to 64 KiB; oversized records receive a suitably sized block. Do not clear
whole byte blocks or construct temporary complete strings. Check lengths and
alignment arithmetic before allocating. No eviction or relocation occurs.

Measure R2 alone against the baseline, then its incremental effect with R1
fixed ON. Keep all directory, record header, alignment, block-tail, lock and
growth costs. Preserve old views across block/page growth and same-slot races.
Counterexamples include sparse, dense/random, short-string, large-output and
same-stripe concurrent first-fill workloads. A simpler paged string cache is
an alternative only if measured costs or correctness concerns justify it.

### R3: borrow raw interface type lists

Remove the persistent interface-ID vectors. Resolve ClassDef through the
existing type_def_idx and read its raw TypeList. Provide a count/index view
which widens uint16 IDs on access. The interface matcher uses the view without
creating another vector; the Hungarian matching state and one-to-one semantics
stay unchanged. Materialize IDs only when producing ClassBean output.

Measure independently. Cover empty lists, interface-dense classes, competing
matchers, and repeated complex interface queries. Other metadata and relation
tables remain separate candidates rather than being folded into this test.

## Final comparisons and bounded workload scope

The adverse workloads narrow the final candidates. Measure R3 alone, the
revised R1 with fast cache hits, R1+R3, and their combination with H1/H2/H3.
Each has twelve main and six confirmation pairs for one and eleven passes,
using the original all-flags-off machine code as control. Raw descriptor-input
lookup, paged descriptor storage and body alignment are excluded from the
preferred combinations because of demonstrated time regressions. Preserve
their code and evidence as default-OFF experiments. Direct comparisons measure
combined costs without adding isolated percentages. All switches stay OFF.

Native-only counterexamples use six balanced pairs per case. Fixed repetition
counts are 64 for large output, 256 for wide-name and same-name-prefix lookups,
100,000 for narrow/hot lookup, and 2,000 for interfaces. Preserve split positive
and negative request timings as well as complete lifecycle and process peaks.
Lookup setup warms the selected hit; first-call timing is labeled accordingly.
A changed workload executable has a separate source/hash manifest and links
the already frozen core archive, so its instrumentation is consistent across
variants. These synthetic measurements are not QQ or Android-runtime scores.

## Counterexample-triggered revision

The first native counterexamples show a large repeated-lookup regression for
R1 and a repeated-large-output regression for R2. Before direct final QQ
combinations, separate raw cross-reference comparison from raw input lookup,
and test whether aligning R2 character bodies fixes the output regression.
Retain the current immutable artifacts and all adverse samples. Only these
concrete findings justify further tuning; no generic new compression project
is being opened. R3 still awaits its QQ comparison.

The body-alignment test failed to resolve R2 repeated output. One final bounded
revision separates published cache hits from cold descriptor construction.
This fixes the observed R1 lookup regression but leaves R2 output slower. No
additional storage design is part of this round. Final work is artifact-specific
correctness, component checks, and QQ comparison of the narrowed candidates.
