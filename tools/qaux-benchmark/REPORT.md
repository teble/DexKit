# QQ 9.3.55 / QAuxiliary suitability assessment

The sample is suitable for a large real-world DexKit corpus. Its useful unit is
a feature's discovery workflow, including dependencies and validation between
queries. Treating all calls as independent searches would lose important work.

## Inputs inspected

QAuxiliary was not present under the supplied Android project directory or the
nearby project search. A fresh shallow checkout of `cinit/QAuxiliary` was created
at `/Users/teble/project/android/QAuxiliary`, pinned to
`01801ffd013c95781dd360704adf48dc42ee8aa6` (2026-09-13). Its DexKit submodule points
to `dff66e8eff15512ac9a2d03cf3ef23de338bd167`; this assessment instead uses the
local DexKit baseline `1d936bd9efa38e00b7336ef112e73ba4048324e6`. QAuxiliary itself
was inspected, not built or installed.

The supplied APK was downloaded and verified against the server's content
length and MD5, ZIP CRCs, and a locally recorded SHA-256. Android package metadata
identifies QQ 9.3.55, versionCode 15900, package `com.tencent.mobileqq`.

| Measurement | Value |
| --- | ---: |
| APK bytes | 389,727,209 |
| Root DEX files | 41 |
| Uncompressed DEX bytes | 411,819,364 |
| Class definitions, summed across DEX files | 423,236 |
| Method-ID entries, summed across DEX files | 2,600,031 |

Method-ID counts include references and cross-DEX duplication; this is not a
count of unique method definitions. File hashes and per-DEX counts are recorded
in `/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/apk-manifest.json`.

## Source structure

`DexKitTarget.kt` defines 150 target objects: 115 `UsingStr`, 24
`UsingStringVector`, five `UsingDexKitBridge`, and six `UsingDexkit` placeholders
whose discovery is implemented elsewhere. All 139 literal string targets were
extracted into 149 groups, with no unsupported definitions skipped.

The backend batches required string targets with `SimilarRegex`, unions version
alternatives, then runs specialized finders serially and verifies candidates.
The initialization scheduler collects preparation steps only for enabled hooks,
deduplicates them, adds a batch step, orders steps by priority, and checks whether
each is still needed before running it. A persistent descriptor cache changes
which steps execute. These are necessary parts of a future captured workload.

## Executed query workflows

The final full replay is `replay-02`, with four query workers, two passes and one
shared bridge. All results and selected descriptors were identical between
passes; no API errors occurred. Seven of ten discovery workflows resolved their
required query results. This does not certify hook installation or host behavior.

| Workflow | Dependency or constraint | QQ 9.3.55 result |
| --- | --- | --- |
| AIOMsgItem_initContentDescription | String seed, then use its descriptor as a caller constraint | `AIOMsgItem.l1()` -> `AIOMsgItem.y1(): String` |
| EmotionDetailAi | Find class by two strings, then restrict a Boolean/numeric method query to those classes | `emotionintegrate.al.n(): boolean` |
| Hd_HideEmoReplyLayout_Method | Find template class, then restrict a View/string method query | `template.g.i(): View` |
| BlockPicByMd5_LoadImagePathV2 | Package, nine parameters, five strings and return type | Empty |
| BlockPicByMd5_PicPathResolverV2 | Method signature plus nested invoke signature | `aio.utils.ap.a(PicElement): String` |
| ReplyNoAtHook | Three ordered signature alternatives, followed by an optional invoke/field query | First alternative and optional query each returned one result; older alternatives skipped |
| MultiForwardAvatarHook | Declared class, no arguments, void return, invokes setOnClickListener | Empty |
| SettingEntryHook | Exact string discovery, cardinality check | One processor method; later reflection excluded |
| HideMiniAppPullEntry | String batch, Java-side class/signature verification, then invoke fallback | First candidate failed verification; fallback found `Conversation.initMiniAppView()` |
| AutoReceiveOriginalPhoto_cache_miss | NT method, optional class-cache lookup, caller/invoke query, visibility query | NT first step empty; remaining steps not executed |

The string batch produced raw candidates for 94 of 139 target definitions.
Forty-five had no raw candidates. These definitions span versions and host
variants; neither number is a supported-feature count. Postfilters and the
enabled-hook profile have not been replayed for this full target superset.

The MiniApp flow is particularly useful: its first query returned a method in
`ContactsViewController`, but QAuxiliary's class/signature validation rejected
it. Only then did the call-relationship fallback run and resolve the target.
Both candidate materialization and validation must remain in this workflow.

## Current misses

Separate diagnostic probes were saved in `diagnostics-01`; they do not alter
the original query corpus or count as resolved features.

- Image loading: removing the signature restriction found one method with all
  five strings. Its first parameter is
  `com.tencent.mobileqq.qqui.widget.RoundBubbleImageView`, while the original
  query requires `android.widget.ImageView`. This explains a concrete mismatch
  in the original exact parameter constraint.
- Avatar listener: the named class is present as DexKit class data, but the
  complete original method constraint returned no results. The exact failing
  component of that constraint has not been isolated.
- Original photo: the resource-name string appears in
  `QQLayerCommonOriginPicSection.onInitView(View)`. No method matched both that
  string and `rootView`, even after removing the method-name constraint. The
  four-step flow is therefore not fully exercised by this APK/source pair.

Compatible APK versions or a separately reviewed query update are needed to
exercise currently unreachable branches. Keep a pinned original-query profile
and record any updated profile separately.

## Validation and limits

The clean arm64 worktree passed `:dexkit:cmakeBuild`, `:dexkit:jar` and
`:dexkit:test`: 68 tests, zero failures/errors/skips. The test pipeline also built
the demo APK and its Android native dependencies. An earlier attempt in the
existing checkout failed at `copyLibrary` because arm64 and x86_64 generated
libraries had the same flattened filename; the clean worktree avoided mixing
these outputs. No production build configuration was changed.

Replay used arm64 JBR 17.0.6 and a Release `-O3 -DNDEBUG` native build with
internal metrics disabled. `replay-02` observed a 356.9 ms bridge creation,
666.5 ms close, and 1,765,949,440-byte whole-process maximum RSS. These are smoke
observations from one process, including JVM/reporting effects, not statistical
benchmarks or a native-only memory measurement. No Android device run or
architecture A/B comparison was performed.

This corpus is a good starting point for batch-string work, narrow follow-up
queries, caller/invoke relations, field constraints and conditional serial
flows. Keep the controlled demo for deep Boolean and injective-matching edge
cases that this real workload does not comprehensively exercise.

## Local artifacts

- Worktree: `/Users/teble/project/android/DexKit-qaux-benchmark`.
- Branch: `codex/qaux-real-world-benchmark`.
- Tools: `tools/qaux-benchmark/{extract.py,QueryReplay.java,run.py,README.md}`.
- APK and per-DEX manifest: `/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55`.
- Extracted targets: `qaux-extracted/targets.json` under that data directory.
- Full final replay: `replay-02/report.json` and `replay-02/run-metadata.json`.
- Relaxed diagnostics: `diagnostics-01/report.json`.
- Baseline build output: `baseline-build.log`.

The original DexKit working tree has no source edits. The experimental worktree
contains the extraction/replay tools and this report; no engine optimization
has been implemented.
