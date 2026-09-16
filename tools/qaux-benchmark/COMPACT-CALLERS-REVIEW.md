# Compact caller source review

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

Pro reviewed the GitHub increment from
`6fc9595137773c20d8fc98c44f37c16146bb48f4` to
`b28699b90f3cc555429cf4e3f4e8f48490d6c17e`, including the new index/builder,
counting and binding integration, publication barrier, caller matcher, build
options, diagnostics, new checks/oracle and staged workload. It did not execute
tests or measurements. The complete response was read and checked locally.

## Findings and handling

- No confirmed answer, overlapping-segment or early-publication defect was
  identified for legal inputs under the existing warmup contract. A resolved
  target is a defined member and cannot also be a forwarding source. Source
  cursors are separate, and preassigned target segments preserve local-first
  and source/work-list order independently of worker completion.
- Confirmed checking-policy mismatch: common.h disables DEXKIT_CHECK under
  NDEBUG. The initial release build still replayed import lengths but its end
  comparisons were absent. The correction enables these internal consistency
  checks in debug and DIAGNOSTICS builds and removes the validation-only replay
  from normal release. Size arithmetic and whole-array write bounds stay active.
  This is not exception or OOM recovery.
- Add a successfully resolved reference with no callers. The revised fixture
  declares and references bUnused without invoking it. The checker verifies its
  binding and empty raw row through all initialization orders; the Python
  validator compares logged targets with the author's explicit expected bindings.
- Independent raw rows are only one part of correctness. The Python validator
  does not independently decode the full FlatBuffer tail. The outer validation
  compares the complete dump bytes between control and compact builds as well.
- Count/cursor and enlarged pending-record capacity must be included in build
  storage. The initial count-phase final_edges=0 means final storage is not yet
  allocated, not that no invocation edges were counted.
- Cold/full preparation is not caller-exclusive time. Memory snapshots and
  their remaining harness objects are described in the execution plan. Counted
  construction, deletion of the late empty instruction walk, and span borrowing
  are one measured increment; total time does not separate their contributions.

The source review applies to b28699b. The checking-policy correction, zero-count
case and additional source-count fixture are subsequent local changes verified
by the final validation results, not a claim that Pro inspected another commit.
