# Ordered next-round experiments

Status: active. Equal and StartWith measurements and independent confirmation
are complete. Batch boolean containment and one using-field requirement are
complete, as are contiguous forward-field rows. The final reverse-only
instruction-walk experiment is active. Every new mechanism remains an
independent experiment against the nine-switch control.

## Controls and sequence

Use the existing nine-switch combination as the incremental control. Keep
new mechanisms behind independent default-OFF build options. Each comparison
uses the same query semantics, source, compiler and frozen inputs. Preserve
the original QQ replay and separate diagnostic runs from timed runs.

1. Attribute string work, then compare one explicit Equal in ordinary
   FindMethod/FindClass: current AC, direct byte comparison and a per-DEX
   sorted-pool ID. Start with nonempty ASCII and ignoreCase=false.
2. Extend that isolated comparison to one explicit StartWith, using ordinary
   find workloads. Batch, multiple requirements, Contains, empty and unsupported
   patterns retain their existing paths. Do not infer a narrower predicate
   from current Contains witnesses.
3. Continue the remaining independently ranked mechanisms from
   NEXT-ROUND-REVIEW: batch boolean containment, one using-field requirement,
   contiguous forward-field rows, and the reverse-only instruction walk.
   The batch containment change is separate from string ID lookup; all batch
   APIs remain excluded from ID/range matching.

For each candidate: establish native ordered-result controls and adversarial
cases, run the frozen QQ query verification, measure finite paired batches
and confirm any conclusion independently. Include construction, preparation,
output and close; retain workload-specific regressions. Check host/JVM and
Android builds for native changes. Use the authorized Pro conversation for
source and evidence review after meaningful increments.

## Progress

* Completed an 8-second native stack sample during an 80-pass diagnostic QQ
  replay of the unchanged current combination. Java and sampling exited zero.
  ParseText and StoreEmits are visible below ordinary usingStrings matching;
  batch work is also substantial. Waiting-thread samples prevent treating the
  raw sample counts as CPU percentages or predicted API gains.
* Implemented default-OFF direct and ID prototypes. The rebuilt control is
  byte-identical to the existing nine-switch native artifact. The initial
  prototypes pass 34 method and 34 class queries against independently encoded
  constant rows and complete ordered serialized control results, including
  cold/full/repeated/concurrent sequences. The range oracle covers all 65,536
  UTF-16 code units and malformed probe boundaries.
* Both initial prototypes preserve the frozen QQ results, ordering and branch
  flow in two-pass verification. The independent oracle was corrected to retain
  duplicate class definitions while deduplicating method descriptors, matching
  the distinct existing public result policies.
* Hoisted Equal/StartWith dispatch out of reference loops before timing, and
  wired the experimental switches through desktop and Android Gradle builds.
  Rebuilding this revision and adding wide-pool checks precede paired timing.
* Completed the source review and fixed DEX-ID narrowing and accidental bulk
  fixture witnesses before formal measurement; see SINGLE-STRING-REVIEW.
  The corrected 32-bit per-DEX publication component passes eight concurrent
  callers at IDs 0 and 65536. Wide ordered oracles, ASan/UBSan, 11 frozen QQ
  rounds, both 71-test JVM runs and both four-ABI AAR builds pass.
* Separate diagnostics confirm one plan per query and one range per visited
  DEX across multiple tasks. A QQ SettingEntry query allocates 3,976 logical
  plan/entry bytes, performs 656 pool comparisons/1,691 decoded units across
  41 DEXes, and visits 55,665 reference IDs. These are work counts, not timing
  estimates; ordinary prefilter and batch AC are outside this trace scope.
* Started balanced six-pair QQ 1/11-round and bounded native measurements,
  followed by an independently ordered confirmation batch. Corrected native
  fixtures cover first/last witnesses, bulk misses, sparse access, class
  matching and unchanged Contains/multiple-requirement paths.
* Completed all 48 Equal sweeps (576 process samples), including independent
  confirmation and DIRECT/ID head-to-head comparisons. Both paths improve QQ
  repeated queries; ID's additional Equal API improvement is measurable, while
  the extra whole-QQ benefit is not consistently resolved. Long late/miss
  workloads favor ID; sparse access favors DIRECT. See SINGLE-STRING-RESULTS.
* Added proper-prefix bulk witnesses and eleven additional query cases for
  the isolated StartWith phase. The 45 method/45 class cases include prefix
  OR/NOT, overlapping requirements, non-ASCII/NUL fallback and unique findFirst.
  Prefix extension remains outside every Batch API.
* Completed all 48 StartWith sweeps (576 process samples), standalone and
  combined ordered-result checks, ASan/UBSan, both JVM configurations and both
  four-ABI AAR builds. ID benefits long late-prefix matching, regresses sparse
  access and has no confirmed extra broad-positive or broad lifecycle benefit.
  QQ's incremental PREFIX-on guard has no stable lifecycle change. Retain the
  small ID multiple-prefix fallback peak cost. See SINGLE-STRING-RESULTS.
* Completed independent batch containment: ten sweeps / 120 samples, expanded
  36-case small/wide ordered checks, standalone/ASan/UBSan, eleven frozen QQ
  rounds, 71 JVM tests and four-ABI AAR assembly. QQ's 149-group repeated API
  improves 4.45% in confirmation, while whole-QQ lifecycle and process peak
  remain unresolved. The high-overlap method/class fixture improves; no-hit
  guards do not establish a change. Keep this separate from later field work.
* Completed one using-field requirement: 18 sweeps / 216 samples, independent
  small/short/long and unresolved-field oracles, standalone/ASan/UBSan, relation
  lifecycle checks, eleven frozen QQ rounds, 71 JVM tests and four-ABI AAR.
  Native long/short improvements repeat, while QQ has no confirmed gain.
  Preserve its 11-pass confirmation slowdown and nested-class peak increase.
  The selector collision and unresolved-field oracle gap were corrected;
  SINGLE-FIELD-RESULTS and SINGLE-FIELD-REVIEW record the scope and evidence.
* Completed contiguous forward-field rows: 22 sweeps / 264 samples, all 48
  queries plus every unique-fixture getter row, standalone/ASan/UBSan and held
  span relation checks, eleven QQ rounds with/without real final RW, 71 JVM
  tests and four Android ABIs. Original QQ lifecycle and peak gains repeat;
  native long-row peak increases also repeat. Keep the warm-query regressions
  and unresolved consumer/tail intervals. A disk-full interruption was recovered
  before formal timing; failed and successful records remain separate. See
  COMPACT-FIELDS-RESULTS and COMPACT-FIELDS-REVIEW. RW-only remains independent.
