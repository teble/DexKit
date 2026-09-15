# Explicit match modes versus observed string witnesses

The user correctly pointed out that counting explicit Contains/Equal/StartWith
modes does not measure the potential coverage of sorted-pool lookup. Default
Contains can be used by a caller looking for a complete constant or a prefix.
Equal and StartWith can share prefix-bound search, with a full-equality check
for Equal, while retaining their different predicates.

This audit refines [the next-round plan](NEXT-ROUND-REVIEW.md). It supplies
current-corpus evidence, not a new performance measurement or proof of caller
intent. These observations are a local follow-up to the completed Pro review;
Pro has not reviewed this new audit.

## Inputs and verification

* QQ 9.3.55 APK SHA-256:
  `851242d139bb01ed8c787eadc30d7ec391437de550c697b5f4c65c32ec84286f`.
* QAuxiliary: `01801ffd013c95781dd360704adf48dc42ee8aa6`.
* Current nine-switch native snapshot:
  `2347a204a16ecf3b0855189ee2c3e62b27e8c1cdff476146ecb7964e6d7422ba`.
* Source adapter: the unchanged QueryReplay.java at `f5fe221`.

A generated diagnostic copy records MethodData.usingStrings; class results
concatenate their declared methods' getter entries. It changes only
ordinary literal usingStrings calls between Contains, Equals and StartsWith.
Batch groups, explicit addEqString, structural filters and chain logic stay
on their original query paths. The original adapter and native library are
the controls, and all pinned APK/JDK/JAR/native identities are checked.

The completed three runs each produced 17 stages with zero API errors.
Contains reproduces the frozen independent verification's ordered results,
multiplicity, returned batch keys and feature outcomes. Equals and StartsWith
produce the same complete ordered results and branch flow in this fixture.
Diagnostic getter calls alter cache work, so timing fields in these runs are
not used as latency evidence.

## Ordinary matcher findings

Of nine default-Contains call sites in chains(), seven execute in this sample.
Five return candidates, and two are empty. Two ReplyNoAt fallback call sites
are not reached. The separate diagnostics() profile's three call sites are
not part of these runs.

Each of the five nonempty query stages has a complete-equality witness for
every requested string in every current result:

| Ordinary query stage | Default-Contains strings | Current results |
| --- | --- | ---: |
| AIOMsgItem / find_seed | senderUid, peerUid | 1 method |
| EmotionDetailAi / find_classes | MsgEmoticonPreviewData, doRestoreSaveInstanceState | 1 class |
| Hd_HideEmoReplyLayout / find_classes | AIOReceiverBubbleTemplate, msgTailContainer | 1 class |
| Hd_HideEmoReplyLayout / find_method_in_classes | msgTailContainer | 1 method |
| ReplyNoAtHook / reply_new_signature | mContext, senderUid | 1 method |

These are nine condition occurrences and seven distinct strings, not five
single-string queries. ReplyNoAt's method also references senderUidStr, while
the separate complete senderUid constant is sufficient for its equality
condition. A returned method can therefore have both equality and prefix
witnesses without needing the broader mode to retain that method.

BlockPicByMd5_LoadImagePathV2 and AutoReceiveOriginalPhoto's nt_on_init_view
are empty under the original structural filters. Their equal empty results
do not establish a useful semantic narrowing opportunity. The explicitly
Equal SettingEntry query remains the initial single-requirement control.

## Batch corpus: candidate-level witness coverage

The earlier count of 183 Contains and four Equal atoms among 149 groups was
an explicit-mode count. It cannot justify treating the corpus as having no
potential equality or prefix coverage.

For each original raw candidate and each condition in its AND group, this
audit checks whether at least one used constant is equal to the pattern,
starts with it, or only contains it at a later position. The categories below
are mutually exclusive; prefix-sufficient excludes equal-sufficient.

| Coverage of the original group's current candidates | Contains condition occurrences |
| --- | ---: |
| Equal witness in every candidate | 93 |
| Prefix witness in every candidate, but Equal loses at least one | 7 |
| At least one candidate requires a nonprefix witness | 15 |
| No current group candidates; no positive evidence | 68 |
| Total | 183 |

At group level, all conditions have sufficient Equal witnesses in 76 groups,
prefix witnesses in another six, while 15 require Contains and 52 are empty.
These group totals include the four originally explicit Equal atoms. Counts
are not deduplicated pattern counts, all-pool prevalence or runtime cost shares.
QAuxiliary's subsequent target/host filters are not applied to this raw batch.

Concrete examples:

* android.permission.SEND_SMS has a full-equality witness in all six current
  CDialogUtil candidates; FlashPicHelper does in its one candidate.
* https://p.qpic. has prefix witnesses in all three CFavEmoConst candidates,
  including the complete constant https://p.qpic.cn/, and no equality witness.
* createPicMessage is a prefix of constants such as
  "createPicMessageToShow : error groupUin:" in its one candidate.
* attrs returns 317 raw candidates. Equal witnesses cover 176 and prefix
  witnesses cover 178; narrowing loses candidates that only use strings such
  as ", attrs.keys=". It cannot be classified as an Equal condition merely
  because a full attrs constant also exists elsewhere.
* debug.conf has three candidates, but one requires a nonprefix occurrence;
  ipv6_debug.config is an observed counterexample.

The [machine-readable summary](evidence/next-round/string-semantics/summary.json)
includes every group, per-condition coverage, bounded string examples and
pinned QAuxiliary source links. Getter-entry counts are not execution counts.

## Implications for optimization

The first investigation should examine ordinary matchers' call sites and
actual witnesses, including the default-Contains cases above. Caller intent
and compatibility across supported APK versions are needed before changing a
caller's declared predicate. One version's same results do not authorize the
engine to reinterpret default Contains as Equal or StartWith.

Keep two independent experiment dimensions:

1. Query semantics: original Contains versus an explicitly narrowed query.
   This audit establishes equality of current fixture results for the executed
   ordinary queries, not universal equivalence.
2. Engine implementation: current AC, direct comparison, or per-DEX ID/range
   lookup, all evaluated with the same declared query semantics and native
   baseline. A change in predicate and implementation together cannot be
   reported as an implementation-only gain.

For an unchanged Contains predicate, an Equal or prefix ID witness can safely
prove a positive match. A missing witness cannot prove failure: interior
matches still require the original Contains path. Such a speculative positive
shortcut adds preparation and row work to misses, which may dominate sparse
queries. Following the user's cost objection, exclude this speculative shortcut
from the first round. Keep mixed Contains/EndWith batches on their existing AC
path, and first evaluate predicates composed entirely of Equal/StartWith.
Pure Equal and a mixture of Equal/StartWith are eligible too; purity lets the
implementation omit AC but does not guarantee a gain over AC or direct checks.

An explicit exact/prefix conjunct might instead reject a candidate before any
AC work, which is a separate selective-filtering hypothesis. In a batch,
rejecting one group does not reject its other independent Contains groups.
Require evidence of actual avoided scanning before pursuing this mixed case.

The user's subsequent scope decision excludes all batch APIs from the
ID/range experiment, even pure Equal/StartWith batches. The batch audit above
remains evidence about observed strings, not an implementation target. Focus
on ordinary FindMethod/FindClass usingStrings, starting with one explicit
Equal or StartWith condition. Ordinary simple usingStrings conditions currently
also use AC; only the fallback calls IsStringMatched, whose Contains branch
uses KMP and whose Equal/StartWith branches perform direct comparisons.
The implementation comparison must follow that actual dispatch path.

## Reproduction

Use the saved metadata of the existing confirmation run to resolve and verify
the frozen inputs. Choose fresh output paths:

```bash
python3 tools/qaux-benchmark/audit_string_semantics.py \
  --reference-run /path/to/qq-master-normal-confirm-p11/pair-00-current-combo \
  --output /path/to/new-string-semantics
python3 tools/qaux-benchmark/summarize_string_semantics.py \
  --run /path/to/new-string-semantics \
  --output /path/to/new-string-semantics/summary.json
```

The runner uses the recorded macOS JDK and library paths. The retained
[manifest](evidence/next-round/string-semantics/manifest.json) records input
and output hashes; complete getter dumps remain with the local diagnostic run.
This work did not implement binary search or change the frozen benchmark.
