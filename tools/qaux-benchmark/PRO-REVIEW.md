# Pro consultation record

Conversation: https://chatgpt.com/c/6aa81c3c-a104-83ee-b6fe-8bcdd4732ff3

This phase used three bounded, read-only reviews in the already-authorized
conversation. Each request named an actual commit pushed to `teble/DexKit`.
No code was pushed to LuckyPray/DexKit. Pro did not run the APK, builds or
benchmarks, and did not independently recalculate the raw measurement samples.
Its complete replies were read and the consequential claims checked locally.

| Review | Source provided and reported as read | Consequential findings | Local response |
| --- | --- | --- | --- |
| Baseline and hypothesis selection | `dcc9c08`: runner, adapter, expected contract, scope/report; original lazy slots and batch scan | Missing report/pass guards, hidden validation overhead, returned-key normalization and incomplete build identity could mislead; prioritize lazy directories, one compact relation, then diagnose string reuse | Separated verify/measure, froze actual returned keys, added immutable build manifests and paired sampling, measured cache representation and scan reuse |
| H1 and attribution | `e909316`: updated harness/probe, H1 source and tests, diagnostic implementation | H1 publication did not introduce an identified blocker; JDK identity and failed-probe handling needed guards; H2 should append directly with checked ranges; sampled duplicate time was not a speedup bound | Added JDK/library identity and environment controls, invalid-probe guards, H2 range checks, exhaustive metadata checks, independently seeded scan samples and clock controls |
| H2/H3 implementation and limited conclusions | `fd5de49`: compact index, cache publication/read paths, negative memo, shared budget, tests and H1/H2 evidence | No identified new normal-path semantic/budget blocker; H3 scope and quota definition were accurate; concurrent quota contention and allocation-failure recovery needed direct checks; local memo object allocation differed from production | Added 16 rounds of eight competing jobs and targeted nothrow allocation failure; used stack memo storage in the local trie test; added empty/tiny-scope cases; finished all promised full-result, A/B and independent confirmation runs |

The final budget checks and measurement confirmation were completed locally
after the last review. The checks-only update rebuilt an identical production
H3 library hash, so it did not silently replace the timed candidate.

The adopted conclusion is bounded: H1/H2 support lower representation cost,
and H3 supports repeated negative-heavy method batches on this corpus. None
establishes arbitrary-query acceleration, combined gains, Android device
performance or justification for a larger engine rewrite in this phase.
The work stops after these three evaluated hypotheses, rather than requiring
Pro agreement or a mandatory speedup.
